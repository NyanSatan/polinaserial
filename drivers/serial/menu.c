#include <CoreFoundation/CoreFoundation.h>
#include <string.h>
#include <unistd.h>

#include <app/app.h>
#include <term.h>
#include <misc.h>

#include "menu.h"
#include "iokit.h"

#define PERIOD      (0.2)
#define START_CHAR  'a'

#define SLOT_COUNT  (26)

struct slot_hdr {
#define SLOT_HEAD_IDX   (0xFF)
    uint8_t prev;
    uint8_t next;
#define SLOT_FLAG_ALLOCATED (1 << 0)
    uint16_t flags;
};

struct slot {
    struct slot_hdr hdr;
    uint64_t id;
    serial_dev_t dev;
};

static struct slot slots[SLOT_COUNT] = { 0 };
static struct slot_hdr slot_head = { SLOT_HEAD_IDX, SLOT_HEAD_IDX, 0 };

static struct slot_hdr *slot_hdr_by_idx(int idx) {
    if (idx == SLOT_HEAD_IDX) {
        return &slot_head;
    }

    REQUIRE_PANIC(idx < SLOT_COUNT);

    return &slots[idx].hdr;
}

static void slot_find_free(struct slot_hdr **hdr, int *idx) {
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (!(slots[i].hdr.flags & SLOT_FLAG_ALLOCATED)) {
            *hdr = &slots[i].hdr;
            *idx = i;

            return;
        }
    }

    panic("no more free slots available for serial menu");
}

static struct slot *slot_acquire(bool tail) {
    struct slot_hdr *new = NULL;
    int idx = 0;

    slot_find_free(&new, &idx);

    if (tail) {
        new->prev = slot_head.prev;
        new->next = SLOT_HEAD_IDX;
        slot_hdr_by_idx(slot_head.prev)->next = idx;
        slot_head.prev = idx;
    } else {
        new->next = slot_head.next;
        new->prev = SLOT_HEAD_IDX;
        slot_hdr_by_idx(slot_head.next)->prev = idx;
        slot_head.next = idx;
    }

    new->flags |= SLOT_FLAG_ALLOCATED;

    return (struct slot *)new;
}

static void slot_release(struct slot *to_release) {
    struct slot_hdr *_to_release = &to_release->hdr;

    slot_hdr_by_idx(_to_release->next)->prev = _to_release->prev;
    slot_hdr_by_idx(_to_release->prev)->next = _to_release->next;

    _to_release->flags &= ~SLOT_FLAG_ALLOCATED;
}

#define SLOT_ITERATE(_name, _code) \
    do { \
        int __idx = slot_head.next; \
        while (__idx != SLOT_HEAD_IDX) { \
            struct slot * _name = (struct slot *)slot_hdr_by_idx(__idx); \
            _code \
            __idx = _name->hdr.next; \
        } \
    } while (0);

static bool started = false;

static const char spinner_lut[] = {'/', '-', '\\', '|'};
#define SPINNER_LUT_CNT (sizeof(spinner_lut) / sizeof(*spinner_lut))

static void spinner(bool update) {
    static int i = 0;

    char out[2] = { '\r' };
    out[1] = spinner_lut[i];

    write(STDOUT_FILENO, out, sizeof(out));

    if (update) {
        if (++i == SPINNER_LUT_CNT) {
            i = 0;
        }
    }
}

static void draw() {
    term_clear_page();

    app_version();
    app_print_cfg();

    int index = 0;
    SLOT_ITERATE(_curr, {
        if (index == 0) {
            POLINA_INFO("\nSelect a device from the list below (or Ctrl+C to exit):");
        }

        serial_dev_t *curr = &_curr->dev;

        POLINA_MISC_NO_BREAK("\t(%c) ", index + START_CHAR);

        if (*curr->usb_name) {
            POLINA_INFO_NO_BREAK("%s", curr->usb_name);

            if (*curr->tty_suffix) {
                POLINA_INFO_NO_BREAK("-%s", curr->tty_suffix);
            }

            POLINA_MISC_NO_BREAK(" on ");
        }

        POLINA_MISC("%s", curr->callout);

        index++;
    });

    if (index == 0) {
        POLINA_INFO("\nwaiting for devices, press Ctrl+C to exit...\n");
    }

    spinner(false);
}

static void timer_callback(CFRunLoopTimerRef timer, void *info) {
    spinner(true);
}

typedef enum {
    MENU_DEVICE_SELECTED = 0,
    MENU_CANCELED
} menu_outcome_t;

static menu_outcome_t outcome = -1;
static serial_dev_t *selected = NULL;

static void keyboard_callback(CFFileDescriptorRef fd, CFOptionFlags options, void *info) {
    char c = 0;
    REQUIRE_PANIC(read(STDIN_FILENO, &c, 1) == 1);

    switch (c) {
        case '\x03': {
            outcome = MENU_CANCELED;
            break;
        }

        default: {
            int idx = c - START_CHAR;
            if (idx < SLOT_COUNT) {
                SLOT_ITERATE(curr, {
                    if (idx-- == 0) {
                        selected = &curr->dev;
                        break;
                    }
                });

                if (!selected) {
                    goto cont;
                }

                outcome = MENU_DEVICE_SELECTED;
                break;
            }

            goto cont;
        }
    }

exit:
    CFRunLoopStop(CFRunLoopGetCurrent());
    return;

cont:
    write(STDOUT_FILENO, "\a", 1);
    CFFileDescriptorEnableCallBacks(fd, kCFFileDescriptorReadCallBack);
}

static void serial_dev_event_cb(io_service_t service, uint64_t id, bool added) {
    if (added) {
        struct slot *slot = slot_acquire(started);
        slot->id = id;
        iokit_serial_dev_from_service(service, &slot->dev);
    } else {
        SLOT_ITERATE(curr, {
            if (curr->id == id) {
                slot_release(curr);
                goto out;
            }
        });

        panic("unknown serial device got disconnected");
    }

out:
    if (started) {
        draw();
    }
}

serial_dev_t *menu_pick() {
    CFRunLoopRef loop = CFRunLoopGetCurrent();

    REQUIRE_NOERR(iokit_register_serial_devices_events(serial_dev_event_cb), fail);

    CFRunLoopTimerRef timer_loop = CFRunLoopTimerCreate(kCFAllocatorDefault, PERIOD, PERIOD, 0, 0, timer_callback, NULL);
    CFRunLoopAddTimer(loop, timer_loop, kCFRunLoopDefaultMode);

    CFFileDescriptorRef fd = CFFileDescriptorCreate(kCFAllocatorDefault, STDIN_FILENO, false, keyboard_callback, NULL);
    CFFileDescriptorEnableCallBacks(fd, kCFFileDescriptorReadCallBack);

    CFRunLoopSourceRef keyboard_loop = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, fd, 0);
    CFRunLoopAddSource(loop, keyboard_loop, kCFRunLoopDefaultMode);

    term_hide_cursor();
    started = true;

    draw();

    CFRunLoopRun();

    iokit_unregister_serial_devices_events();

    CFRunLoopRemoveTimer(loop, timer_loop, kCFRunLoopDefaultMode);
    CFRelease(timer_loop);

    CFRunLoopRemoveSource(loop, keyboard_loop, kCFRunLoopDefaultMode);
    CFRelease(keyboard_loop);
    CFFileDescriptorInvalidate(fd);
    CFRelease(fd);

    term_clear_line();
    term_show_cursor();

    if (outcome == MENU_DEVICE_SELECTED) {
        return selected;
    }

fail:
    return NULL;
}

static serial_dev_list_t *_devices = NULL;

serial_dev_t *serial_dev_find_by_callout(const char *callout) {
    ll_iterate(_devices, serial_dev_list_t *, curr, {
        if (strcmp(callout, curr->dev.callout) == 0) {
            return &curr->dev;
        }
    });

    return NULL;
}

int serial_find_devices() {
    if ((_devices = iokit_serial_find_devices())) {
        return 0;
    }

    return -1;
}

void serial_dev_list_destroy() {
    if (_devices) {
        ll_destroy((ll_t **)&_devices, NULL);
        _devices = NULL;
    }
}
