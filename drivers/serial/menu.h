#ifndef SERIAL_MENU_H
#define SERIAL_MENU_H

#include <limits.h>

typedef struct {
    char tty_name[128];
    char tty_suffix[64];
    char callout[256];
    char usb_name[128];
} serial_dev_t;

/*
 * Populates a list of currently available serial devices on the system.
 * It's a wrapper around lower level routines (only IOKit backend available this far)
 */
int  serial_find_devices();
void serial_dev_list_destroy();


/*
 * try to find serial device by dev node path,
 * useful for getting more information for cases
 * when user explicitly requested specific dev node
 * and not menu
 */
serial_dev_t *serial_dev_find_by_callout(const char *callout);

/* draw and handle menu, returns serial_dev_t that user selects */
serial_dev_t *menu_pick();

#endif
