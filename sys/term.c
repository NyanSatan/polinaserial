#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <halt.h>
#include <term.h>
#include <tty.h>
#include <misc.h>

#define SCROLL_UP_FMT   "\033[%dS"
#define CLEAR_TERM      "\033[H\033[J"
#define CLEAR_LINE      "\033[2K\r"
#define HIDE_CURSOR     "\033[?25l"
#define SHOW_CURSOR     "\033[?25h"

int term_scroll() {
    struct winsize w = { 0 };
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        return -1;
    }

    printf(SCROLL_UP_FMT, w.ws_row);

    return 0;
}

void term_clear_page() {
    printf(CLEAR_TERM);
}

void term_clear_line() {
    printf(CLEAR_LINE);
}

void term_hide_cursor() {
    printf(HIDE_CURSOR);
}

void term_show_cursor() {
    printf(SHOW_CURSOR);
}

static bool attrs_were_saved = false;
static struct termios saved_attrs = { 0 };

int term_save_attrs() {
    if (attrs_were_saved) {
        POLINA_ERROR("trying to save terminal attributes again");
        return -1;
    }

    REQUIRE_NOERR(tty_get_attrs(STDIN_FILENO, &saved_attrs), fail);

    attrs_were_saved = true;

    return 0;

fail:
    POLINA_ERROR("couldn't save terminal attributes");
    return -1;
}

int term_restore_attrs() {
    if (attrs_were_saved) {
        return tty_set_attrs(STDIN_FILENO, &saved_attrs);
    } else {
        return 0;
    }
}

int term_set_raw(bool filter_return) {
    int ret = -1;
    struct termios new_attrs = { 0 };

    REQUIRE_NOERR(tty_get_attrs(STDIN_FILENO, &new_attrs), out);
    cfmakeraw(&new_attrs);

    if (filter_return) {
        new_attrs.c_oflag  = OPOST;
        new_attrs.c_oflag |= ONLCR;
    }

    new_attrs.c_cc[VTIME] = 1;
    new_attrs.c_cc[VMIN] = 0;

    REQUIRE_NOERR(tty_set_attrs(STDIN_FILENO, &new_attrs), out);
    ret = 0;

out:
    return ret;
}
