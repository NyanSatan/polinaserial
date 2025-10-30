#include <stdio.h>
#include <stdint.h>
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
#include <app/driver.h>
#include <io.h>
#include <halt.h>
#include <event.h>
#include <misc.h>
#include <tty.h>

static int init(int argc, const char *argv[]) {
    return 0;
}

static uint8_t data[16 * 1024 * 1024] = { 0 };
static uint32_t idx = 0;

static int preflight() {
    arc4random_buf(data, sizeof(data));
    return 0;
}

const static uint8_t others[] = { 0x7, 0x9, 0xa, 0xb, 0xd, 0x1b };

static void *random_loop(void *arg) {
    POLINA_SET_THREAD_NAME("random driver loop");

    driver_event_cb_t cb = arg;
    app_event_t event = APP_EVENT_DISCONNECT_DEVICE;
    uint8_t buf[IO_MAX_BUFFER_SIZE];

    while (idx < sizeof(data)) {
        uint32_t r = data[idx];
        for (uint32_t i = 0; i < r; i++) {
            uint8_t c = data[idx++] % 0x6e;
            if (c < 0x10) {
                c = others[c % sizeof(others)];
            } else {
                c += 0x10;
            }

            buf[i] = c;
        }

        if (r == 0) {
            idx++;
            continue;
        }

        if (cb(buf, r) != 0) {
            event = APP_EVENT_ERROR;
            goto out;
        }

        idx += r;
        usleep(data[idx] * 10);
    }

out:
    app_event_signal(event);

out_no_signal:
    return NULL;
}

pthread_t loop_thr = NULL;

static int start(driver_event_cb_t out_cb, driver_conn_cb_t conn_cb) {
    conn_cb();
    pthread_create(&loop_thr, NULL, random_loop, out_cb);
    return 0;
}

static int restart() {
    panic("not supported in 'random' driver");
    return 0;
}

static int _write(uint8_t *buf, size_t len) {
    return 0;
}

static int quiesce() {
    pthread_kill(loop_thr, 0);
    return 0;
}

static void log_name(char name[], size_t len) {
    strlcpy(name, "RANDOM", len);
}

static void print_cfg() {
}

static void help() {
    printf("\tthis is just for testing\n");
}

DRIVER_ADD(
    random,
    init,
    preflight,
    start,
    restart,
    _write,
    quiesce,
    log_name,
    print_cfg,
    help,
    ""
);
