#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <halt.h>
#include <lolcat.h>
#include <misc.h>
#include <compiler.h>

#define COLOR_256  "\x1B[38;5;"
#define STOP       "m"
#define RESET      "\x1B[0m"

#define SKIP_COUNT  (4)

struct lolcat_color {
    char clr[4];
    uint8_t raw;
    uint8_t len;
};

/*
 * pre-generate ASCII values for numbers in the LUT,
 * so we don't have to convert them at runtime
 */
#define LOLCAT_CLR(_clr) \
    {#_clr, _clr, CONST_STRLEN(#_clr)}

static const struct lolcat_color lolcat_lut[] = {
    LOLCAT_CLR(214), LOLCAT_CLR(208), LOLCAT_CLR(208), LOLCAT_CLR(203),
    LOLCAT_CLR(203), LOLCAT_CLR(198), LOLCAT_CLR(198), LOLCAT_CLR(199),
    LOLCAT_CLR(199), LOLCAT_CLR(164), LOLCAT_CLR(164), LOLCAT_CLR(128),
    LOLCAT_CLR(129), LOLCAT_CLR(93),  LOLCAT_CLR(93),  LOLCAT_CLR(63),
    LOLCAT_CLR(63),  LOLCAT_CLR(63),  LOLCAT_CLR(33),  LOLCAT_CLR(33),
    LOLCAT_CLR(39),  LOLCAT_CLR(38),  LOLCAT_CLR(44),  LOLCAT_CLR(44),
    LOLCAT_CLR(49),  LOLCAT_CLR(49),  LOLCAT_CLR(48),  LOLCAT_CLR(48),
    LOLCAT_CLR(83),  LOLCAT_CLR(83),  LOLCAT_CLR(118), LOLCAT_CLR(118),
    LOLCAT_CLR(154), LOLCAT_CLR(154), LOLCAT_CLR(184), LOLCAT_CLR(184),
    LOLCAT_CLR(178), LOLCAT_CLR(214)
};

#define LOLCAT_LUT_CNT  (sizeof(lolcat_lut) / sizeof(*lolcat_lut))

/*
 * save previous index, so we don't have to
 * lolcatify another character with the same color
 */
static int lut_pos_prev = 0;
static int lut_pos = 0;

/* we shift initial color of a new line to tilt the rainbow */
static int lut_line_pos = 0;
static int lut_pos_skip = 0;

/* increase LUT index or go back to 0 if we reached the end */
static int lut_pos_increment_simple(int curr) {
    return (curr == LOLCAT_LUT_CNT - 1) ? 0 : ++curr;
}

/* increase LUT index after we repeated current one SKIP_COUNT times */
static int lut_pos_increment(int curr) {
    lut_pos_skip++;

    if (lut_pos_skip != SKIP_COUNT) {
        return curr;
    }

    lut_pos_skip = 0;

    return lut_pos_increment_simple(curr);
}

static bool lolcat_printable(char c) {
    return c > 0x20 && c < 0x7F;
}

/* 
 * IDK if this `unlikely` has a real performance benifit,
 * profiling gave mixed results, but won't hurt to keep it
 */
#define PUSH(__data, __len) \
    do { \
        if (unlikely(_out_len + __len > max_len)) { \
            panic("\nmax_len: %zu, _out_len + __len: %zu", max_len, _out_len + __len); \
        } \
        memcpy(out + _out_len, __data, __len); \
        _out_len += __len; \
    } while (0)


int lolcat_push_one(const uint8_t *data, size_t len, uint8_t *out, size_t *out_len) {
    size_t max_len = *out_len;
    size_t _out_len = 0;

    if (lut_pos_prev != lut_pos) {
        if (lut_pos_prev == -1 || lolcat_lut[lut_pos_prev].raw != lolcat_lut[lut_pos].raw) {
            const struct lolcat_color *clr = &lolcat_lut[lut_pos];
            PUSH(COLOR_256, CONST_STRLEN(COLOR_256));
            PUSH(clr->clr, clr->len);
            PUSH(STOP, CONST_STRLEN(STOP));
        }

        lut_pos_prev = lut_pos;
    }

    PUSH(data, len);

    lut_pos = lut_pos_increment(lut_pos);

    *out_len = _out_len;

    return 0;
}

int lolcat_push_ascii(const uint8_t *data, size_t data_len, uint8_t *out, size_t *out_len) {
    size_t max_len = *out_len;
    size_t _out_len = 0;

    for (size_t i = 0; i < data_len; i++) {
        uint8_t c = data[i];

        if (lolcat_printable(c)) {
            size_t __out_len = max_len - _out_len;
            REQUIRE_NOERR(lolcat_push_one(&c, sizeof(c), out + _out_len, &__out_len), fail);
            _out_len += __out_len;
        } else {
            /* XXX handle tabs as well? */
            switch (c) {
                case ' ':
                    PUSH(&c, sizeof(c));
                    lut_pos = lut_pos_increment(lut_pos);
                    break;

                case '\n':
                    lut_line_pos = lut_pos_increment_simple(lut_line_pos);
                    lut_pos = lut_line_pos;
                    /* break is missed intentionally */

                default:
                    PUSH(&c, sizeof(c));
                    break;
            }
        }
    }

    *out_len = _out_len;

    return 0;

fail:
    return -1;
}

void lolcat_refresh() {
    lut_pos_prev = -1;
}

void lolcat_init() {
    lut_pos_prev = -1;
    lut_pos = arc4random_uniform(LOLCAT_LUT_CNT);
    lut_line_pos = lut_pos;
}

void lolcat_reset() {
    write(STDOUT_FILENO, RESET, CONST_STRLEN(RESET));
    fflush(stdout);
}
