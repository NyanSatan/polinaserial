#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>

#include <misc.h>
#include <seq.h>

static struct {
    off_t idx;
    seq_type_t type;
    bool shall_finish;

    uint8_t utf8_cnt_left;

    esc_seq_state_t esc_state;
    uint16_t esc_count;
} ictx = { 0 };

/* line break & tab technically fall under C0 codes, but to simplify things... */
static bool is_printable_character(uint8_t c) {
    return (c >= 0x20 && c <= 0x7e) || c == '\t' || c == '\n';
}

static bool is_c0_character(uint8_t c) {
    return (c >= 0x07 && c <= 0x0d);
}

/* checks if a char could be an initial UTF-8 byte */
static bool is_unicode_character(uint8_t c, uint8_t *len) {
    if (!(c & 0b10000000)) {
        return false;
    }

    static const struct {
        uint8_t shift;
        uint8_t ref;
        uint8_t cnt;
    } utf8_lut[] = {
        {5, 0b110, 2},
        {4, 0b1110, 3},
        {3, 0b11110, 4}
    };

    for (int i = 0; i < 3; i++) {
        if ((c >> utf8_lut[i].shift) == utf8_lut[i].ref) {
            *len = utf8_lut[i].cnt;
            return true;
        }
    }

    return false;
}

/* checks if a char is n-th (2nd+) UTF-8 character */
static bool is_unicode_nth_character(uint8_t c) {
    return (c & 0b11000000) == 0b10000000;
}

#define ESC_SEQUENCE_MAX_LEN    (64)

static bool is_escape_character(uint8_t c) {
    return c == 0x1B;
}

enum {
    kEscOutcomeAbort = 0,
    kEscOutcomeContinue,
    kEscOutcomeFinish
};

/* process CSI escape sequence and understand what to do next */
static int handle_escape_seq(uint8_t c) {
    if (ictx.esc_count == 0) {
        panic("CSI state, but count is zero (?!)");
    }

    if (ictx.esc_count > ESC_SEQUENCE_MAX_LEN) {
        POLINA_WARNING("escape sequence too long");
        return kEscOutcomeAbort;
    }

    int ret = kEscOutcomeAbort;

    switch (ictx.esc_state) {
        case kEscSeqGoing: {
            if (ictx.esc_count == 1) {
                if (c != '[') {
                    ret = kEscOutcomeAbort;
                } else {
                    ret = kEscOutcomeContinue;
                }
            } else if (c >= 0x20 && c <= 0x3F) {
                ret = kEscOutcomeContinue;
            } else if (c >= 0x40 && c <= 0x7E) {
                ret = kEscOutcomeFinish;
            } else {
                ret = kEscOutcomeAbort;
            }

            goto out;
        }

        case kEscSeqNone:
            panic("escape seq state machine bug");
    }

out:
    if (ret != kEscOutcomeContinue) {
        ictx.esc_count = 0;
        ictx.esc_state = kEscSeqNone;
    } else {
        ictx.esc_count++;
    }

    return ret;
}

/* we were successfully processing a long sequence, but it broke mid-way */
static void handle_broken_seq(seq_ctx_t *ectx) {
    /*
     * if it's in the very beginning (like, start was in previous package),
     * just behave like we started normally
     */
    if (ictx.idx == 0) {
        ictx.type = kSeqNone;
        return;
    }

    /* if it was normal printable ASCII text, return it to user */
    if (ictx.type == kSeqNormal) {
        ectx->type = ictx.type;
        ictx.shall_finish = true;
        return;
    }

    /* otherwise treat the sequence as unknown */
    ictx.type = kSeqUnknown;
    ictx.idx++;
}

static void handle_ongoing_seq(seq_ctx_t *ectx) {
    ictx.idx++;
}

/* populate finished sequence data into ctx & prepare to return */
static void handle_finished_seq(seq_ctx_t *ectx) {
    ectx->type = ictx.type;
    ictx.type = kSeqNone;
    ictx.shall_finish = true;
    ictx.idx++;
}

/* called when unknown sequence is finally over */
static void handle_unknown_seq() {
    ictx.type = kSeqNone;
    ictx.shall_finish = true;
}

/* long sequences can be split across multiple packages */
static void handle_interrupted_seq(seq_ctx_t *ectx) {
    ectx->type = ictx.type;
}

/*
 * C0 control sequences are not really sequences,
 * but still require special handling in app code,
 * so let's act accordingly
 */
static void handle_control_seq(seq_ctx_t *ectx) {
    if (ictx.idx == 0) {
        ictx.type = kSeqNone;
        return;
    }

    ectx->type = ictx.type;
    ictx.type = kSeqNone;
    ictx.shall_finish = true;
}

/* nightmare code */
int seq_process_chars(seq_ctx_t *ectx, const uint8_t *buf, size_t len) {
    ectx->type = kSeqNone;
    ictx.shall_finish = false;
    ictx.idx = 0;

    REQUIRE_PANIC(len > 0);

    while (ictx.idx < len) {
        uint8_t c = buf[ictx.idx];

        if (ictx.type == kSeqNone || ictx.type == kSeqUnknown) {
            if (is_printable_character(c)) {
                ictx.type = kSeqNormal;
            } else if (is_c0_character(c)) {
                ictx.type = kSeqControl;
            } else if (is_unicode_character(c, &ictx.utf8_cnt_left)) {
                ictx.type = kSeqUnicode;
                ictx.utf8_cnt_left--;
                ectx->has_utf8_first_byte = true;
            } else if (is_escape_character(c)) {
                ictx.type = kSeqEscapeCSI;
                ictx.esc_state = kEscSeqGoing;
                ictx.esc_count = 1;
            } else {
                ictx.type = kSeqUnknown;
                ectx->type = kSeqUnknown;
            }

            if (ectx->type == kSeqUnknown && ictx.type != kSeqUnknown) {
                handle_unknown_seq();
            } else {
                ictx.idx++;
            }

        } else {
            switch (ictx.type) {
                case kSeqNormal: {
                    if (!is_printable_character(c)) {
                        handle_broken_seq(ectx);
                    } else {
                        handle_ongoing_seq(ectx);
                    }

                    break;
                }

                case kSeqUnicode: {
                    if (is_unicode_nth_character(c)) {
                        if (ictx.idx == 0) {
                            ectx->has_utf8_first_byte = false;
                        }

                        ictx.utf8_cnt_left--;

                        if (ictx.utf8_cnt_left == 0) {
                            handle_finished_seq(ectx);
                        } else {
                            handle_ongoing_seq(ectx);
                        }
                    } else {
                        handle_broken_seq(ectx);
                    }

                    break;
                }

                case kSeqEscapeCSI: {
                    switch (handle_escape_seq(c)) {
                        case kEscOutcomeAbort: {
                            handle_broken_seq(ectx);
                            break;
                        }

                        case kEscOutcomeContinue: {
                            handle_ongoing_seq(ectx);
                            break;
                        }

                        case kEscOutcomeFinish: {
                            handle_finished_seq(ectx);
                            break;
                        }
                    }

                    break;
                }

                case kSeqControl: {
                    handle_control_seq(ectx);
                    break;
                }

                default:
                    panic("char state machine bug (ictx.state: %d, ectx.state: %d, idx: %d, char: 0x%02x)", ictx.type, ectx->type, ictx.idx, c);
            }
        }

        if (ictx.shall_finish) {
            goto out;
        }
    }

    handle_interrupted_seq(ectx);

out:
    return ictx.idx;
}
