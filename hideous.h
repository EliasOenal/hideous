/*      Copyright (C) 2012
 *      Elias Oenal     (EliasOenal@gmail.com)
 *      License:        CC0 1.0 Universal
 */

#ifndef HIDEOUS_H
#define HIDEOUS_H

#include <stdint.h>
#include <stddef.h>

// hideous is a library for "driverless" usb communication.
// Ignore all the haters telling you HID wasn't meant to be used for whatever you're doing,
// as long as is works and has a great user experience you're doing it right! :)

// hideous makes heavy use of int32_t since it matches Java's int size. This allows easier interfacing via JNI.

#include <stdint.h>

typedef void* hideous_handle;

enum HIDEOUS_EVENTS
{
    GENERAL_ERROR = 0,          // SNAFU
    RECEIVED_STOP_SIGNAL = 1,   // Shutting down as told
    DEVICE_PLUGGED_IN = 2,      // New device, yay :D
    DEVICE_UNPLUGGED = 3,       // Unplugged :/
    RECEIVED_DATA = 4,          // new data
    TRANSMITTED_DATA = 5,       // sending data succeeded
    ERROR_TRANSMITTING_DATA = 6      // no waii!
};

// pid/vid are actually only uint16_t
// 32bit signed is for easier Java/JNI interfacing as well as using -1 as wildcard
/*! @function   hideous_init
    @abstract   Creates a new hideous_handle
    @param      -1 for wildcard or PID
    @param      -1 for wildcard or VID
    @param      NULL for wildcard or serial number as zero terminated char array
    @return     hideous_handle or NULL in case of an error
*/
hideous_handle hideous_init(int32_t pid, int32_t vid, char* serial_number); // NULL in case of error


/*! @function   hideous_destroy_all_things
    @abstract   Destroys a hideous_handle
    @param      hideous_handle you want to destroy
    @return     0 if successful
*/
int32_t hideous_destroy_all_things(hideous_handle handle); // Destroy she said!


/*! @function   hideous_open_first_device
    @abstract   Opens the first device found
    @param      hideous_handle
    @return     0 if successful
*/
int32_t hideous_open_first_device(hideous_handle handle);


/*! @function   hideous_register_hotplug_events
    @abstract   Once called the event listener will receive hotplug events
    @param      hideous_handle
    @return     0 if successful
*/
int32_t hideous_register_hotplug_events(hideous_handle handle);

/*! @function   hideous_blocking_event_listener
    @abstract   Blocks till it receives any new event
    @param      hideous_handle
    @param      int32_t containing the maximum time to block in ms, -1 for unlimited
    @return     0 if successful
*/
/*int32_t hideous_blocking_event_listener(hideous_handle handle, int32_t timeoutInMs);*/ // you probably want to call me from
                                                                                     // a thread since I tend to block

/*! @function   hideous_callback_event_listener
    @abstract   Registers a callback to be used for event handling.
                It will be called from hideous' inherent pthread
    @param      The hideous handle
    @param      The callback function-pointer. It must take exactly one parameter
                of type int32_t which represents the status code for the event received
    @return     0 if successful
*/
int32_t hideous_callback_event_listener(hideous_handle handle, void (*callback_function)(int32_t status));



/*! @function   hideous_stop_event_listener
    @abstract   Drops all pending events and stops any further ones.
                Also disables all callbacks and exits the blocking listener
    @param      hideous_handle
    @return     0 if successful
*/
//int32_t hideous_stop_event_listener(hideous_handle handle);        // quit the event listener



/*! @function   hideous_register_receive_data_event
    @abstract   Registers for incoming data events
    @param      hideous_handle
    @return     0 if successful
*/
//int32_t hideous_register_receive_data_event(hideous_handle handle);



/*! @function   hideous_register_send_data_event
    @abstract   Registers the send data event which gets called every time data is sent, this can succeed or fail
    @param      hideous_handle
    @return     0 if successful
*/
//int32_t hideous_register_send_data_event(hideous_handle handle);




#endif // HIDEOUS_H
