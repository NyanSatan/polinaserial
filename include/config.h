#ifndef POLINA_CONFIG_H
#define POLINA_CONFIG_H

#include <stdbool.h>
#include <unistd.h>

typedef struct {
    bool filter_return;
    bool filter_delete;
    bool filter_iboot;
    bool filter_lolcat;

    useconds_t delay;
    bool retry;

    bool logging_disabled;
} polina_config_t;

#endif
