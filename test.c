#include "hideous.h"
#include "stdio.h"

void call_me(int32_t status);

int main(void)
{

    //    hid_handle* myH = Init();

    //    // So I have to use the OSX runloop? meh :/
    //    CFRunLoopGetCurrent();
    //    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5/*sec*/, FALSE);
    //    //CFRunLoopRun();
    //    //CFRunLoopStop();

    //    destroyAllThings(myH);

    // flexbox has 0x0483, 0x5750
//        hideous_handle myHandle = hideous_init(0x5750, 0x0483, NULL);
    hideous_handle myHandle = hideous_init(-1, -1, NULL);
    hideous_callback_event_listener(myHandle, call_me);
    hideous_register_hotplug_events(myHandle);
    hideous_open_first_device(myHandle);
    printf("Hello World!\n");

//    hideous_register_hotplug_events(myHandle);
//    printf("Hello World!\n");

    char arr[100];
//    scanf("%s",arr);

//    hideous_stop_event_listener(myHandle);
    hideous_destroy_all_things(myHandle);


    printf("Hello World!\n");
    fflush(stdout);
    return 0;
}

void call_me(int32_t status)
{
    printf("got called! status: %d\n", status);
    fflush(stdout);
}
