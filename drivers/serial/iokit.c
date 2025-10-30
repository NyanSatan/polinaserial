#include <stdio.h>
#include <string.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/USBSpec.h>

#include <app/app.h>
#include <misc.h>
#include <ll.h>

#include "menu.h"
#include "iokit.h"

/* because it's "not available on iOS" */
const mach_port_t ___kIOMasterPortDefault asm("_kIOMasterPortDefault");

#define STR_FROM_CFSTR(_cftsr, _target, _len) \
    REQUIRE_PANIC(CFStringGetCString(_cftsr, _target, _len, kCFStringEncodingUTF8));

#define __cf_release(_ref) \
    if (_ref) { \
        CFRelease(_ref); \
        _ref = NULL; \
    }

#define __iokit_release(_obj) \
    if (_obj) { \
        IOObjectRelease(_obj); \
        _obj = IO_OBJECT_NULL; \
    }

static io_registry_entry_t iokit_get_parent_with_class(io_service_t service, const char *class) {
    io_name_t name;
    io_registry_entry_t prev_parent = IO_OBJECT_NULL;
    io_registry_entry_t parent = service;

    do {
        if (parent != service) {
            prev_parent = parent;
        }

        if (IORegistryEntryGetParentEntry(parent, kIOServicePlane, &parent) != KERN_SUCCESS) {
            __iokit_release(prev_parent);
            return IO_OBJECT_NULL;
        }

        if (IOObjectGetClass(parent, name) != KERN_SUCCESS) {
            POLINA_WARNING("couldn't get IOObject name");
            __iokit_release(parent);
            return IO_OBJECT_NULL;
        }

        if (prev_parent) {
            __iokit_release(prev_parent);
        }

    } while (strcmp(name, class) != 0);

    return parent;
}

int iokit_serial_dev_from_service(io_service_t service, serial_dev_t *device) {
    int ret = -1;
    CFMutableDictionaryRef properties = NULL;
    CFStringRef tty_name = NULL;
    CFStringRef tty_suffix = NULL;
    CFStringRef callout_device = NULL;
    CFStringRef usb_name = NULL;
    io_registry_entry_t usb_parent = IO_OBJECT_NULL;
    CFMutableDictionaryRef usb_parent_properties = NULL;

    if (IORegistryEntryCreateCFProperties(service, &properties, kCFAllocatorDefault, kNilOptions) != KERN_SUCCESS) {
        POLINA_ERROR("couldn't get IOService properties");
        goto out;
    }

    tty_name = CFDictionaryGetValue(properties, CFSTR("IOTTYDevice"));
    if (!tty_name) {
        POLINA_ERROR("couldn't get IOTTYDevice");
        goto out;
    }

    tty_suffix = CFDictionaryGetValue(properties, CFSTR("IOTTYSuffix"));
    if (!tty_suffix) {
        POLINA_ERROR("couldn't get IOTTYSuffix");
        goto out;
    }

    callout_device = CFDictionaryGetValue(properties, CFSTR("IOCalloutDevice"));
    if (!callout_device) {
        POLINA_ERROR("couldn't get IOCalloutDevice");
        goto out;
    }

    usb_parent = iokit_get_parent_with_class(service, "IOUSBHostDevice");
    if (!usb_parent) {
        usb_parent = iokit_get_parent_with_class(service, "IOUSBDevice");
    }

    if (usb_parent) {
        if (IORegistryEntryCreateCFProperties(usb_parent, &usb_parent_properties, kCFAllocatorDefault, kNilOptions) == KERN_SUCCESS) {
            usb_name = CFDictionaryGetValue(usb_parent_properties, CFSTR(kUSBProductString));
        } else {
            POLINA_WARNING("couldn't get USB IOService properties");
        }
    }

    STR_FROM_CFSTR(tty_name, device->tty_name, sizeof(device->tty_name));

    if (CFStringGetLength(tty_suffix) > 0) {
        STR_FROM_CFSTR(tty_suffix, device->tty_suffix, sizeof(device->tty_suffix));
    } else {
        *device->tty_suffix = '\0';
    }

    STR_FROM_CFSTR(callout_device, device->callout, sizeof(device->callout));

    if (usb_name) {
        STR_FROM_CFSTR(usb_name, device->usb_name, sizeof(device->usb_name));
    }

    ret = 0;

out:
    __cf_release(properties);
    __cf_release(usb_parent_properties);
    __iokit_release(usb_parent);

    return ret;
}

static void iokit_serial_device_added_cb(void *ref, io_iterator_t iterator) {
    iokit_event_cb_t cb = ref;
    io_object_t object;
    uint64_t id;

    while ((object = IOIteratorNext(iterator))) {
        IORegistryEntryGetRegistryEntryID(object, &id);
        cb(object, id, true);
        __iokit_release(object);
    };
}

static void iokit_serial_device_removed_cb(void *ref, io_iterator_t iterator) {
    iokit_event_cb_t cb = ref;
    io_object_t object;
    uint64_t id;

    while ((object = IOIteratorNext(iterator))) {
        IORegistryEntryGetRegistryEntryID(object, &id);
        cb(object, id, false);
        __iokit_release(object);
    };
}

static IONotificationPortRef notification_port = NULL;
static IONotificationPortRef termination_notification_port = NULL;

int iokit_register_serial_devices_events(iokit_event_cb_t cb) {
    CFRunLoopRef notification_run_loop = CFRunLoopGetCurrent();
    
    CFMutableDictionaryRef matching_dict = IOServiceMatching("IOSerialBSDClient");;

    mach_port_t master_port = ___kIOMasterPortDefault;

    notification_port = IONotificationPortCreate(master_port);
    CFRunLoopAddSource(
        notification_run_loop,
        IONotificationPortGetRunLoopSource(notification_port),
        kCFRunLoopDefaultMode
    );

    io_iterator_t iterator = IO_OBJECT_NULL;

    CFRetain(matching_dict);
    kern_return_t ret = IOServiceAddMatchingNotification(
        notification_port,
        kIOMatchedNotification,
        matching_dict,
        iokit_serial_device_added_cb,
        cb,
        &iterator
    );
    
    if (ret != KERN_SUCCESS) {
        POLINA_ERROR("couldn't register add serial device notification");
        goto fail;
    }
    
    iokit_serial_device_added_cb(cb, iterator);
    
    termination_notification_port = IONotificationPortCreate(___kIOMasterPortDefault);
    CFRunLoopAddSource(
        notification_run_loop,
        IONotificationPortGetRunLoopSource(termination_notification_port),
        kCFRunLoopDefaultMode
    );
    
    CFRetain(matching_dict);
    ret = IOServiceAddMatchingNotification(
        termination_notification_port,
        kIOTerminatedNotification,
        matching_dict,
        iokit_serial_device_removed_cb,
        cb,
        &iterator
    );
    
    if (ret != KERN_SUCCESS) {
        POLINA_ERROR("couldn't register remove serial device notification");
        goto fail;
    }
    
    iokit_serial_device_removed_cb(cb, iterator);
    
    __cf_release(matching_dict);
    
    return 0;
    
fail:
    return -1;
}

void iokit_unregister_serial_devices_events() {
    IONotificationPortDestroy(notification_port);
    IONotificationPortDestroy(termination_notification_port);

    notification_port = NULL;
    termination_notification_port = NULL;
}

serial_dev_list_t *iokit_serial_find_devices() {
    serial_dev_list_t *devices = NULL;
    CFMutableDictionaryRef matching_dict = NULL;
    io_iterator_t iterator = IO_OBJECT_NULL;
    io_service_t service = IO_OBJECT_NULL;

    matching_dict = IOServiceMatching("IOSerialBSDClient");
    if (!matching_dict) {
        POLINA_ERROR("couldn't get matching dictionary from IOKit");
        goto fail;
    }

    if (IOServiceGetMatchingServices(___kIOMasterPortDefault, matching_dict, &iterator) != KERN_SUCCESS) {
        POLINA_ERROR("couldn't get matching services");
        goto fail;
    }

    while ((service = IOIteratorNext(iterator))) {
        serial_dev_list_t *item = calloc(sizeof(*item), 1);
        if (!item) {
            POLINA_ERROR("out of memory?!");
            goto fail;
        }

        iokit_serial_dev_from_service(service, &item->dev);
        __iokit_release(service);

        ll_add((ll_t **)&devices, item);
    }

    goto out;

fail:
    ll_destroy((ll_t **)&devices, NULL);

out:
    __iokit_release(iterator);
    __iokit_release(service);

    return devices;
}
