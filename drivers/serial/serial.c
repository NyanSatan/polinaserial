#include <CoreFoundation/CFRunLoop.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <app/app.h>
#include <misc.h>
#include <tty.h>
#include <io.h>

#include "menu.h"
#include "iokit.h"
#include "config.h"
#include "device.h"

static struct {
    bool ready;

    serial_dev_t *picked;
    char callout[PATH_MAX + 1];

    int dev_fd;

    struct termios old_attrs;
    struct termios new_attrs;
    bool attrs_modified;

    driver_event_cb_t out_cb;

    int kq;
    pthread_t data_loop_thr;

    pthread_t restart_loop_thr;
    CFRunLoopRef restart_thr_run_loop;
    bool restart_success;
} ctx = { 0 };

static serial_config_t config = { 0 };

static int init(int argc, const char *argv[]) {
    int ret = -1;

    ctx.dev_fd = -1;

    REQUIRE_NOERR(serial_config_load(argc, argv, &config), out);

    ret = 0;

out:
    return ret;
}

static int preflight() {
    int ret = -1;

    if (!config.device) {
        ctx.picked = menu_pick();
        if (!ctx.picked) {
            goto out;
        }

        strlcpy(ctx.callout, ctx.picked->callout, sizeof(ctx.callout));
    } else {
        if (serial_find_devices() != 0) {
            POLINA_ERROR("couldn't look up serial devices");
            goto out;
        }

        ctx.picked = serial_dev_find_by_callout(config.device);
        strlcpy(ctx.callout, config.device, strlen(config.device) + 1);
    }

    ctx.ready = true;
    ret = 0;

out:
    return ret;
}

#define LOOP_SHUTDOWN_ID    (613)

static void *data_loop(void *arg) {
    POLINA_SET_THREAD_NAME("serial driver data loop");

    driver_event_cb_t cb = arg;
    app_event_t event = APP_EVENT_NONE;
    struct kevent ke = { 0 };
    uint8_t buf[IO_MAX_BUFFER_SIZE];

    REQUIRE_PANIC((ctx.kq = kqueue()) > 0);

    EV_SET(&ke, LOOP_SHUTDOWN_ID, EVFILT_USER, EV_ADD, 0, 0, NULL);
    kevent(ctx.kq, &ke, 1, NULL, 0, NULL);

    EV_SET(&ke, ctx.dev_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(ctx.kq, &ke, 1, NULL, 0, NULL);

    EV_SET(&ke, ctx.dev_fd, EVFILT_VNODE, EV_ADD, NOTE_DELETE, 0, NULL);
    kevent(ctx.kq, &ke, 1, NULL, 0, NULL);

    while (true) {
        memset(&ke, 0, sizeof(ke));
        int i = kevent(ctx.kq, NULL, 0, &ke, 1, NULL);

        if (i == 0) {
            continue;
        }

        if (ke.filter == EVFILT_VNODE && ke.fflags & NOTE_DELETE) {
            close(ctx.dev_fd);
            ctx.dev_fd = -1;
            event = APP_EVENT_DISCONNECT_DEVICE;
            goto out;

        } else if (ke.filter == EVFILT_READ && ke.ident == ctx.dev_fd) {
            ssize_t r = read(ctx.dev_fd, buf, sizeof(buf));

            if (r > 0) {
                if (cb(buf, r) != 0) {
                    event = APP_EVENT_ERROR;
                    goto out;
                }
            }
        } else if (ke.filter == EVFILT_USER && ke.ident == LOOP_SHUTDOWN_ID) {
            goto out_no_signal;
        }
    }

out:
    app_event_signal(event);

out_no_signal:
    close(ctx.kq);
    ctx.kq = -1;

    return NULL;
}

static void close_fd() {
    if (ctx.attrs_modified) {
        tty_set_attrs(ctx.dev_fd, &ctx.old_attrs);
    }

    close(ctx.dev_fd);
    ctx.dev_fd = -1;
}

static int start(driver_event_cb_t out_cb, driver_conn_cb_t conn_cb) {
    int ret = -1;
    struct termios new_attrs = { 0 };

    REQUIRE((ctx.dev_fd = device_open_with_callout(ctx.callout)) != -1, fail);

    REQUIRE_NOERR(tty_get_attrs(ctx.dev_fd, &ctx.old_attrs), fail);
    memcpy(&ctx.new_attrs, &ctx.old_attrs, sizeof(struct termios));

    tty_set_attrs_from_config(&ctx.new_attrs, &config);

    REQUIRE_NOERR(tty_set_attrs(ctx.dev_fd, &ctx.new_attrs), fail);
    ctx.attrs_modified = true;

    REQUIRE_NOERR(device_set_speed(ctx.dev_fd, config.baudrate), fail);

    conn_cb();

    ctx.out_cb = out_cb;

    pthread_create(&ctx.data_loop_thr, NULL, data_loop, out_cb);

    ret = 0;
    goto out;

fail:
    if (ctx.dev_fd != -1) {
        close_fd();
    }

out:
    return ret;
}

static void restart_cb(io_service_t service, uint64_t id, bool added) {
    if (added) {
        serial_dev_t dev = { 0 };
        REQUIRE_NOERR(iokit_serial_dev_from_service(service, &dev), out);
        if (strcmp(dev.callout, ctx.callout) == 0) {
            ctx.restart_success = true;
            CFRunLoopStop(CFRunLoopGetCurrent());
        }
    }

out:
    return;
}

static void *restart_loop(void *arg) {
    POLINA_SET_THREAD_NAME("serial driver restart loop");

    ctx.restart_thr_run_loop = CFRunLoopGetCurrent();
    ctx.restart_success = false;

    REQUIRE_NOERR(iokit_register_serial_devices_events(restart_cb), fail);

    CFRunLoopRun();

    ctx.restart_thr_run_loop = NULL;

    iokit_unregister_serial_devices_events();

    if (ctx.restart_success) {
        REQUIRE((ctx.dev_fd = device_open_with_callout(ctx.callout)) != -1, fail);
        REQUIRE_NOERR(tty_set_attrs(ctx.dev_fd, &ctx.new_attrs), fail);
        REQUIRE_NOERR(device_set_speed(ctx.dev_fd, config.baudrate), fail);

        pthread_create(&ctx.data_loop_thr, NULL, data_loop, ctx.out_cb);

        driver_conn_cb_t cb = arg;
        cb();
    }

    goto out;

fail:
    POLINA_ERROR("reconnection failure");
    app_event_signal(APP_EVENT_ERROR);

out:
    return NULL;
}

static int restart(driver_conn_cb_t cb) {
    ctx.data_loop_thr = NULL;
    pthread_create(&ctx.restart_loop_thr, NULL, restart_loop, cb);
    return 0;
}

static int _write(uint8_t *buf, size_t len) {
    write(ctx.dev_fd, buf, len);
    return 0;
}

static int quiesce() {
    if (ctx.dev_fd != -1) {
        struct kevent ke = { 0 };
        EV_SET(&ke, LOOP_SHUTDOWN_ID, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
        kevent(ctx.kq, &ke, 1, 0, 0, NULL);

        pthread_join(ctx.data_loop_thr, NULL);
        ctx.data_loop_thr = NULL;

        close_fd();
    }

    if (ctx.restart_loop_thr) {
        if (ctx.restart_thr_run_loop) {
            CFRunLoopStop(ctx.restart_thr_run_loop);
            ctx.restart_thr_run_loop = NULL;
        }

        pthread_join(ctx.restart_loop_thr, NULL);
        ctx.restart_loop_thr = NULL;
    }

    serial_dev_list_destroy();

    memset(&ctx, 0, sizeof(ctx));

    return 0;
}

static void log_name(char name[], size_t len) {
    if (!ctx.ready) {
        panic("serial driver uninitialized, but log_name() was called?!");
    }

    if (ctx.picked) {
        strlcpy(name, ctx.picked->tty_name, len);
    } else {
        strlcpy(name, last_path_component(ctx.callout), len);
    }
}

static void print_cfg() {
    serial_print_cfg(&config);
}

DRIVER_ADD(
    serial,
    init,
    preflight,
    start,
    restart,
    _write,
    quiesce,
    log_name,
    print_cfg,
    serial_help,
    SERIAL_ARGUMENTS
);
