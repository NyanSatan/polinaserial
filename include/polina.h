#ifndef POLINA_H
#define POLINA_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct {
    /* maps NL to CRNL in output */
    bool filter_return;

    /* converts 0x7F to 0x08 upon input backspace */
    bool filter_delete;

    /* (try to) print iBoot filename in obfuscated logs */
    bool filter_iboot;

    /* paint the logs as rainbow */
    bool filter_lolcat;

    /* input delay after every character, in microseconds */
    useconds_t delay;

    /* wait until disconnected device appears again */
    bool retry;

    /* if user explicitly asks not to log */
    bool logging_disabled;
} polina_config_t;

/* initialize polinalib internal state (only for library usage) */
int polina_init(polina_config_t *cfg, const char *log_name);

/* brings up polinalib IO threads, doesn't block, `out` is a pipe to read output from */
int polina_start_io(int (*in_cb)(uint8_t c), int out);

/* restore terminal settings, flush log file, free up memory and etc. */
void polina_quiesce(void);

#endif
