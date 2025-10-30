#ifndef POLINA_LOLCAT_H
#define POLINA_LOLCAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* initialize lolcat internal state */
void lolcat_init();

typedef int (*lolcat_handler_t)(
    const uint8_t *data,
    size_t data_len,
    uint8_t *out,
    size_t *out_len
);

/* process ASCII text through lolcat */
int lolcat_push_ascii(
    const uint8_t *data,
    size_t data_len,
    uint8_t *out,
    size_t *out_len
);

/* process multiple bytes as one character, good for Unicode */
int lolcat_push_one(
    const uint8_t *data,
    size_t len,
    uint8_t *out,
    size_t *out_len
);

/* resets color modes and etc. */
void lolcat_reset();

/* 
 * forces to output ANSI color sequence
 * upon next call into lolcat_push_*()
 */
void lolcat_refresh();

#endif
