#ifndef POLINA_TTY_H
#define POLINA_TTY_H

#include <termios.h>

int tty_get_attrs(int fd, struct termios *attrs);
int tty_set_attrs(int fd, struct termios *attrs);

#endif
