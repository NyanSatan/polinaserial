#ifndef POLINA_IO_H
#define POLINA_IO_H

#include <stdint.h>
#include <stddef.h>
#include "polina.h"

#define IO_MAX_BUFFER_SIZE  (1024)

void io_set_config(polina_config_t *cfg);
void io_set_input_cb(int (*cb)(uint8_t c));

void io_user_input_start();

int io_out_cb(uint8_t *in_buf, size_t in_len);

#endif
