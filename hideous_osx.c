#include <stdio.h>
#include <IOKit/hid/IOHIDManager.h>
#include <pthread.h>
#include <hideous.h>
#include <assert.h>

static CFMutableDictionaryRef CreateDictionary(const uint16_t vendorID, const uint16_t productID);
static void Handle_DeviceMatchingCallback(void* inContext, IOReturn inResult, void* inSender,
                                          IOHIDDeviceRef  inIOHIDDeviceRef);
static void Handle_RemovalCallback(void* inContext, IOReturn inResult,void* inSender,
                                   IOHIDDeviceRef inIOHIDDeviceRef);
//static void Handle_OpenedDeviceHasBeenRemoved(void *inContext, IOReturn inResult,
//                                              void *inSender, IOHIDDeviceRef inIOHIDDeviceRef);

static void* hideous_background_thread(void *handle);

// lacks locking - on purpose
static int32_t hideous_stop_event_listener_private(hideous_handle handle);


typedef struct
{
    IOHIDManagerRef hidManager;
    CFMutableDictionaryRef myDict;
    CFSetRef deviceCFSetRef;
    CFRunLoopRef runLoopRef;
    IOHIDDeviceRef deviceHandle;
    CFIndex deviceCount;
    pthread_t backgroundThread;
    int32_t threadJob;
    int32_t processRunloop;
    void* callbackFunction;
    pthread_mutex_t serializeFunctionCallsMutex; // limits function calls
    pthread_mutex_t handleProtectionMutex; // big lock
    pthread_mutex_t wakeUpThreadMutex; // wake up thread - if we've acquired the protection mutex
    // then the thread is sleeping
} hideous_real_handle;


hideous_handle hideous_init(int32_t pid, int32_t vid, char* serial_number)
{
    assert(serial_number == NULL); // for now

    hideous_real_handle* new_handle = malloc(sizeof(hideous_real_handle));
    memset(new_handle, 0x00, sizeof(hideous_real_handle));
    ((hideous_real_handle*)new_handle)->processRunloop = 0;

    pthread_mutex_init(&(((hideous_real_handle*)new_handle)->serializeFunctionCallsMutex), NULL);
    pthread_mutex_lock(&(((hideous_real_handle*)new_handle)->serializeFunctionCallsMutex));
    {
        pthread_mutex_init(&(((hideous_real_handle*)new_handle)->handleProtectionMutex), NULL);
        pthread_mutex_lock(&(((hideous_real_handle*)new_handle)->handleProtectionMutex));
        {

            pthread_mutex_init(&(((hideous_real_handle*)new_handle)->wakeUpThreadMutex), NULL); // init mutex

            ((hideous_real_handle*)new_handle)->threadJob = 1; // make the thread acquire the runLoop

            int status = pthread_create(&(((hideous_real_handle*)new_handle)->backgroundThread),
                                        NULL, hideous_background_thread, new_handle);
            if(status) return 2;

            // No special allocator and no options, and yet we get a free hidManager object :D
            new_handle->hidManager = IOHIDManagerCreate(NULL, kIOHIDOptionsTypeNone);

            if((vid != -1) || (pid != -1))
            {
                //        assert(0); //for now
                // Why do we need a crappy function to match 2 stupid values?
                new_handle->myDict = CreateDictionary(vid, pid);

                // And all I got was this lousy t-shirt...
                IOHIDManagerSetDeviceMatching(new_handle->hidManager, new_handle->myDict);
            }
            else
            {
                new_handle->myDict = CreateDictionary(0,0); // just a dummy so we can destroy it later
                IOHIDManagerSetDeviceMatching(new_handle->hidManager, NULL);
            }


            printf("init\n");
            fflush(stdout);
        }
        pthread_mutex_unlock(&(((hideous_real_handle*)new_handle)->handleProtectionMutex));
    }
    //  Our thread will unlock it, this makes sure no functions are called until the runloop
    //  has been acquired.
    //    pthread_mutex_unlock(&(((hideous_real_handle*)new_handle)->serializeFunctionCallsMutex));

    return (hideous_handle)new_handle;
}

int32_t hideous_register_hotplug_events(hideous_handle handle)
{
    pthread_mutex_lock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));
    {
        pthread_mutex_lock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
        {
            // we command our thread to register hotplug events

            ((hideous_real_handle*)handle)->threadJob = 2; // make the thread register hotplug events
            ((hideous_real_handle*)handle)->processRunloop = 0;
            CFRunLoopStop((((hideous_real_handle*)handle)->runLoopRef));
            pthread_mutex_unlock(&(((hideous_real_handle*)handle)->wakeUpThreadMutex));

            printf("hotplug\n");
            fflush(stdout);
        }
        pthread_mutex_unlock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
    }
    //  Our thread will unlock it, this makes sure no functions are called until the hotplug
    //  events have been registered.
    //    pthread_mutex_unlock(&(((hideous_real_handle*)new_handle)->serializeFunctionCallsMutex));

}

int32_t hideous_open_first_device(hideous_handle handle)
{
    pthread_mutex_lock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));
    {
        pthread_mutex_lock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
        {
            if(((hideous_real_handle*)handle)->deviceCount) // IF we HAVE devices
            {
                IOHIDDeviceRef *devices = calloc(((hideous_real_handle*)handle)->deviceCount,
                                                 sizeof(IOHIDDeviceRef));
                CFSetGetValues(((hideous_real_handle*)handle)->deviceCFSetRef, (const void **)devices);

                IOReturn status = IOHIDDeviceOpen(devices[0], kIOHIDOptionsTypeNone);
                if (status == kIOReturnSuccess)
                {
                    printf("successfully opened device #1!\n");
                }

                free(devices);
            }
        }
        pthread_mutex_unlock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
    }
    pthread_mutex_unlock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));
}


int32_t hideous_destroy_all_things(hideous_handle handle)
{
    printf("starting to destroy all things\n");
    fflush(stdout);
    int32_t returnVal = 0;
    pthread_mutex_lock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));
    {
        pthread_mutex_lock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
        {
            if(handle)
            {

                hideous_stop_event_listener_private(handle);

                if(((hideous_real_handle*)handle)->myDict)
                    CFRelease(((hideous_real_handle*)handle)->myDict);
                free((hideous_real_handle*)handle);

                returnVal = 0;
            }
            else
            {
                // Do nothing
                returnVal = 1;
            }
        }
        pthread_mutex_unlock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
    }
    pthread_mutex_unlock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));

    printf("finished destroying all things\n");
    fflush(stdout);

    return returnVal;
}

//int32_t hideous_stop_event_listener(hideous_handle handle)
//{
//    int32_t status;

//    pthread_mutex_lock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));
//    {
//        pthread_mutex_lock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
//        {
//            status = hideous_stop_event_listener_private(handle);
//        }
//        pthread_mutex_unlock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
//    }
//    pthread_mutex_unlock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));

//    return status;
//}

static int32_t hideous_stop_event_listener_private(hideous_handle handle)
{
    printf("stopping event listener\n");
    fflush(stdout);

    if(!handle) return 1;

    ((hideous_real_handle*)handle)->threadJob = -1;
    ((hideous_real_handle*)handle)->processRunloop = 0;
    CFRunLoopStop((((hideous_real_handle*)handle)->runLoopRef));

    pthread_mutex_unlock(&(((hideous_real_handle*)handle)->wakeUpThreadMutex)); // wake up thread,
    // demanding suicide

    pthread_mutex_unlock(&(((hideous_real_handle*)handle)->handleProtectionMutex)); // passing handle to thread

    printf("waiting for thread\n");
    fflush(stdout);

    if(((hideous_real_handle*)handle)->backgroundThread)
        pthread_join(((hideous_real_handle*)handle)->backgroundThread, NULL);

    ((hideous_real_handle*)handle)->backgroundThread = 0;

    printf("joined thread\n");
    fflush(stdout);

    return 0;
}

int32_t hideous_callback_event_listener(hideous_handle handle, void (*callback_function)(int32_t status))
{
    int32_t returnVal = 0;
    pthread_mutex_lock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));
    {
        pthread_mutex_lock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
        {
            if(!callback_function) return 3;

            if(!(((hideous_real_handle*)handle)->callbackFunction))         // if callback function is undefined
            {
                ((hideous_real_handle*)handle)->callbackFunction = callback_function;
                returnVal = 0;
            }
            returnVal = 1;
        }
        pthread_mutex_unlock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
    }
    pthread_mutex_unlock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));

    printf("callback\n");
    fflush(stdout);
    return returnVal;
}

static void *hideous_background_thread(void* handle)
{
    while(1)
    {
        if(((hideous_real_handle*)handle)->processRunloop)
        {
            // working!
            printf("workin'\n");
            fflush(stdout);
            //            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0/*sec*/, FALSE);
            CFRunLoopRun();
            printf("stopped workin'\n");
            fflush(stdout);
        }
        else
        {
            // go back to sleep until told otherwise
            pthread_mutex_lock(&(((hideous_real_handle*)handle)->wakeUpThreadMutex));
        }

        printf("thread tries to get lock\n");
        fflush(stdout);
        pthread_mutex_lock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
        {
            printf("thread got lock\n");
            fflush(stdout);
            switch(((hideous_real_handle*)handle)->threadJob)
            {
            case 0:
                printf("doing nothin'\n");
                fflush(stdout);
                break;

            case 1: // get runloop
                printf("gettin' runloop\n");
                fflush(stdout);
                ((hideous_real_handle*)handle)->runLoopRef = CFRunLoopGetCurrent();
                ((hideous_real_handle*)handle)->threadJob = 0;

                // unlock functions
                pthread_mutex_unlock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));
                break;

            case 2:
                // registering hotplug events
            {
                printf("registerin' hotplug\n");
                fflush(stdout);

                // Function pointers... -> passing handle to callbacks
                IOHIDManagerRegisterDeviceMatchingCallback(((hideous_real_handle*)handle)->hidManager,
                                                           Handle_DeviceMatchingCallback, handle);
                IOHIDManagerRegisterDeviceRemovalCallback(((hideous_real_handle*)handle)->hidManager,
                                                          Handle_RemovalCallback, handle);

                // I think that arms our callback/interrupt
                IOHIDManagerScheduleWithRunLoop(((hideous_real_handle*)handle)->hidManager,
                                                ((hideous_real_handle*)handle)->runLoopRef, kCFRunLoopDefaultMode);

                // This would get us a device listing, but we don't really care

                IOHIDManagerOpen(((hideous_real_handle*)handle)->hidManager, kIOHIDOptionsTypeNone);

                ((hideous_real_handle*)handle)->deviceCFSetRef = IOHIDManagerCopyDevices(((hideous_real_handle*)handle)->hidManager);
                if(((hideous_real_handle*)handle)->deviceCFSetRef)
                {
                    ((hideous_real_handle*)handle)->deviceCount = CFSetGetCount(((hideous_real_handle*)handle)->deviceCFSetRef);
                    printf("Devices found: %d\n", ((hideous_real_handle*)handle)->deviceCount);
                }
                else
                    printf("No devices found!\n");


                ((hideous_real_handle*)handle)->processRunloop = 1; // get working!

                // unlock functions
                pthread_mutex_unlock(&(((hideous_real_handle*)handle)->serializeFunctionCallsMutex));
                ((hideous_real_handle*)handle)->threadJob = 0;
                break;
            } // end hotplug

            case 3:

                break;

                //            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1/*sec*/, FALSE);
                // we have to stop that again in order to shut down the thread
                // CFRunLoopStop() should do so

                //                ((hideous_real_handle*)handle)->threadJob = 0;
                //                CFRunLoopRun();
                //                break;

            case -1:
                // Disable callbacks and quit
                IOHIDManagerRegisterDeviceMatchingCallback(((hideous_real_handle*)handle)->hidManager, NULL, NULL);
                IOHIDManagerRegisterDeviceRemovalCallback(((hideous_real_handle*)handle)->hidManager, NULL, NULL);

                if(((hideous_real_handle*)handle)->deviceCFSetRef)
                    CFRelease(((hideous_real_handle*)handle)->deviceCFSetRef);
                if(((hideous_real_handle*)handle)->hidManager)
                    CFRelease(((hideous_real_handle*)handle)->hidManager);
                if(((hideous_real_handle*)handle)->deviceCFSetRef)
                    printf("thread does /wrists\n");
                fflush(stdout);
                pthread_exit(NULL);
            }
        }
        pthread_mutex_unlock(&(((hideous_real_handle*)handle)->handleProtectionMutex));
    }

    //            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5/*sec*/, FALSE);
}

static CFMutableDictionaryRef CreateDictionary(const uint16_t vendorID, const uint16_t productID)
{
    // According to the apple documentation I have to create a dictionary in order to filter
    // my PID/VID pair. I don't see why it asks for weird callbacks and I hope NULL will do it.
    CFMutableDictionaryRef Result = CFDictionaryCreateMutable(NULL,0,NULL,NULL);

    // kCFNumberSInt32Type means I compare to an 32bit signed integer. I would actually
    // need a 16bit unsigned one, but apple has none of those. :/
    // I also have no clue what those CFNumbers are and why we need them...
    int32_t tempVendorID = vendorID;
    int32_t tempProductID = productID;

    CFNumberRef vidCFNumber = CFNumberCreate(NULL, kCFNumberSInt32Type, &tempVendorID);
    CFDictionarySetValue(Result,CFSTR(kIOHIDVendorIDKey),vidCFNumber); //that CFSTR makro is fucked up
    CFRelease(vidCFNumber); //And now we get rid of that crap again? O.o
    CFNumberRef pidCFNumber = CFNumberCreate(NULL, kCFNumberSInt32Type, &tempProductID);
    CFDictionarySetValue(Result, CFSTR(kIOHIDProductIDKey),pidCFNumber);
    CFRelease(pidCFNumber);

    printf("dict\n");
    fflush(stdout);
    return Result;
}



// this will be called when the HID Manager matches a new (hot plugged) HID device
static void Handle_DeviceMatchingCallback(
        void *          inContext,       // context from IOHIDManagerRegisterDeviceMatchingCallback
        IOReturn        inResult,        // the result of the matching operation
        void *          inSender,        // the IOHIDManagerRef for the new device
        IOHIDDeviceRef  inIOHIDDeviceRef // the new HID device
        )
{
    printf("%s(context: %p, result: %p, sender: %p, device: %p).\n",
           __PRETTY_FUNCTION__, inContext, (void *) inResult, inSender, (void*) inIOHIDDeviceRef);
    fflush(stdout);

    void (*callbackFunc)(int32_t status) = ((hideous_real_handle*)inContext)->callbackFunction;
    if(((hideous_real_handle*)inContext)->callbackFunction)
        (*callbackFunc)(DEVICE_PLUGGED_IN);
}


// this will be called when a HID device is removed (unplugged)
static void Handle_RemovalCallback(
        void *         inContext,       // context from IOHIDManagerRegisterDeviceMatchingCallback
        IOReturn       inResult,        // the result of the removing operation
        void *         inSender,        // the IOHIDManagerRef for the device being removed
        IOHIDDeviceRef inIOHIDDeviceRef // the removed HID device
        )
{
    printf("%s(context: %p, result: %p, sender: %p, device: %p).\n",
           __PRETTY_FUNCTION__, inContext, (void *) inResult, inSender, (void*) inIOHIDDeviceRef);
    fflush(stdout);

    void (*callbackFunc)(int32_t status) = ((hideous_real_handle*)inContext)->callbackFunction;
    if(((hideous_real_handle*)inContext)->callbackFunction)
        (*callbackFunc)(DEVICE_UNPLUGGED);
}

//static void Handle_OpenedDeviceHasBeenRemoved(void *inContext, IOReturn inResult,
//                                              void *inSender, IOHIDDeviceRef inIOHIDDeviceRef)
//{

//}
