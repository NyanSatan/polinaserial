#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <app/app.h>
#include <misc.h>

#include "device.h"
#include "config.h"

int device_open_with_callout(const char *callout) {
    int fd = open(callout, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd < 0) {
        POLINA_ERROR("device %s is unavailable", callout);
        return -1;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        POLINA_ERROR("device %s is locked", callout);
        return -1;
    }

    return fd;
}

int device_set_speed(int fd, speed_t speed) {
    if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
        POLINA_ERROR("couldn't set speed - %s", strerror(errno));
        return -1;
    }

    return 0;
}

void tty_set_attrs_from_config(struct termios *attrs, serial_config_t *config) {
    attrs->c_cflag |= (CLOCAL | CREAD);
    attrs->c_iflag |= (IGNPAR);
    attrs->c_oflag &= ~OPOST;

    /* data bits */
    attrs->c_cflag &= ~CSIZE;
    switch (config->data_bits) {
        case 8:
            attrs->c_cflag |= CS8;
            break;

        case 7:
            attrs->c_cflag |= CS7;
            break;

        case 6:
            attrs->c_cflag |= CS6;
            break;

        case 5:
            attrs->c_cflag |= CS5;
            break;
    }

    /* stop bits */
    if (config->stop_bits == 2) {
        attrs->c_cflag |= CSTOPB;
    } else {
        attrs->c_cflag &= ~CSTOPB;
    }

    /* parity */
    switch (config->parity) {
        case PARITY_NONE:
            attrs->c_cflag &= ~PARENB;
            break;

        case PARITY_EVEN:
            attrs->c_cflag |= PARENB;
            attrs->c_cflag &= ~PARODD;
            break;

        case PARITY_ODD:
            attrs->c_cflag |= (PARENB | PARODD);
            break;
    }

    /* flow control */
    switch (config->flow_control) {
        case FLOW_CONTROL_NONE:
            attrs->c_iflag &= ~(IXON | IXOFF);
            break;

        case FLOW_CONTROL_HW:
            attrs->c_cflag |= (CCTS_OFLOW | CRTS_IFLOW);
            // missing break is intentional

        case FLOW_CONTROL_SW:
            attrs->c_iflag |= (IXON | IXOFF);
            break;
    }
}
