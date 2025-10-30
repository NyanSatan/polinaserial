#ifndef POLINA_TERM_H
#define POLINA_TERM_H

#include <stdbool.h>

int term_save_attrs();
int term_restore_attrs();

/*
 * scroll a full page to simulate a clean terminal,
 * and yet keep all the content before
 */
int  term_scroll();
void term_clear_page();
void term_clear_line();

void term_hide_cursor();
void term_show_cursor();

int term_set_raw(bool filter_return);

#endif
