#ifndef APP_APP_H
#define APP_APP_H

#include <stdbool.h>

/* prints app's build tag + copyright string */
void app_version();

/* prints configuration, both app & driver specific */
void app_print_cfg();

typedef enum {
    APP_ARG_NOT_CONSUMED = 0,
    APP_ARG_CONSUMED,
    APP_ARG_CONSUMED_WITH_ARG
} app_arg_consumed_t;

/* checks whether argument is consumed by app itself (and not driver) */
app_arg_consumed_t app_config_arg_consumed(char c);

/* (try to) shutdown the app gracefully, doesn't exit by its' own */
int app_quiesce(int ret);

#include <app/driver.h>
#include <app/event.h>
#include <halt.h>

#endif
