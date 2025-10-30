#ifndef APP_DRIVER_H
#define APP_DRIVER_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

/* when driver has data ready to output, call this */
typedef int (*driver_event_cb_t)(uint8_t *buf, size_t len);

/*
 * (re-)connection callback type,
 * such callback is implemented in the app code
 * to notify user about (re-)connection event
 */
typedef int (*driver_conn_cb_t)();

/* driver struct, each driver must declare it via DRIVER_ADD() macro */
typedef struct {
    char name[24];                              // name shown in help menu
    int  (*init)(int argc, const char *argv[]); // early init, process input arguments here
    int  (*preflight)();                        // serial driver invokes menu or looks up serial device here
    int  (*start)(driver_event_cb_t out_cb, driver_conn_cb_t conn_cb);  // actually opens device & starts data loop thread,
                                                                        // must NOT block
    int  (*restart)(driver_conn_cb_t cb);       // implement device reconnection logic here, must NOT block
    int  (*write)(uint8_t *buf, size_t len);    // sends user input into device
    int  (*quiesce)();                          // gracefully shutdowns everything
    void (*log_name)(char name[], size_t len);  // returns name for logging folder
    void (*print_cfg)();                        // prints driver-specific configuration
    void (*help)();                             // prints help menu entry
    char optstring[32];                         // options as for getopt()
} driver_t;

/* declares driver struct, so that app can look it up and call into it */
#define DRIVER_ADD(name, init, preflight, start, restart, write, quiesce, log_name, print_cfg, help, optstring) \
    __attribute__((used, disable_sanitizer_instrumentation)) static const driver_t __driver_##name __attribute__((section("__DATA,__drivers"))) = \
        {#name, init, preflight, start, restart, write, quiesce, log_name, print_cfg, help, optstring};

extern const void *_gDrivers      __asm("section$start$__DATA$__drivers");
extern const void *_gDriversEnd   __asm("section$end$__DATA$__drivers");

static const driver_t *gDrivers = (driver_t *)&_gDrivers;
static const driver_t *gDriversEnd = (driver_t *)&_gDriversEnd;

/* driver iterator, for matching with requested driver & printing help menu */
#define DRIVER_ITERATE(action) \
    for (int i = 0; i < ((void *)gDriversEnd - (void *)gDrivers) / sizeof(driver_t); i++) { \
        const driver_t *curr = gDrivers + i; \
        action \
    }

#endif
