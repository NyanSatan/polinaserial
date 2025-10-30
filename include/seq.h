/*
 * Horrible code to detect various non-ASCII sequences such as:
 *
 *  - Unicode (UTF-8)
 *  - Control codes (C0 - bell & etc.)
 *  - Control sequences (only CSI)
 *  
 * Unrecognized sequences are tried to be processed as-is
 */

#ifndef POLINA_SEQ_H
#define POLINA_SEQ_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    kSeqNone = 0,
    kSeqNormal,
    kSeqControl,
    kSeqEscapeCSI,
    kSeqUnicode,
    kSeqUnknown
} seq_type_t;

typedef enum {
    kEscSeqNone = 0,
    kEscSeqGoing
} esc_seq_state_t;

typedef struct {
    seq_type_t type;
    bool has_utf8_first_byte;
} seq_ctx_t;

/*
 * returns amount of bytes in current sequence,
 * populates ctx struct with additional info,
 * meant to be called over and over
 * until there are no more bytes in current package
 */
int seq_process_chars(seq_ctx_t *ctx, const uint8_t *buf, size_t len);

#endif
