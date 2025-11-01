/* (high-level) IO subsystem, character (post-)processing goes here */

#ifndef POLINA_IO_H
#define POLINA_IO_H

#include <stdint.h>
#include <stddef.h>
#include "polina.h"

#define IO_MAX_BUFFER_SIZE  (1024)

/* pass configuration struct, so we know whether to apply lolcat, disable logging and etc. */
void io_set_config(polina_config_t *cfg);

/* set the callback to send user-inputted data to hardware */
void io_set_input_cb(int (*cb)(uint8_t c));

/* start user input handler thread, doesn't block */
void io_user_input_start();

/* call this with data received from hardware to postprocess and print it in terminal */
int io_out_cb(uint8_t *in_buf, size_t in_len);

#endif
