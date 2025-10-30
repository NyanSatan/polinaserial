#ifndef POLINA_IBOOT_H
#define POLINA_IBOOT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint64_t hmac;
    char *file;
} iboot_hmac_config_t;

typedef struct {
    const char *file;
    int line;
} iboot_log_line_t;

/* all the text data passing through serial must go here */
int iboot_push_data(const uint8_t *data, size_t data_len);

/* if we reached \r char, we shall try to match HMAC against the DB */
bool iboot_trigger(iboot_log_line_t *line);

/* actually print matched filename + line */
int iboot_output_file(iboot_log_line_t *line, uint8_t *buf, size_t *out_len);

/* loads additional HMACs from file */
int iboot_load_aux_hmacs(const char *path);
void iboot_destroy_aux_hmacs();

#endif
