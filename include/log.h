#ifndef POLINA_LOG_H
#define POLINA_LOG_H

#include <stdint.h>
#include <stddef.h>

/* creates logging directory and file and starts log flushing thread */
int  log_init(const char *dev_name);

/* writes text to log */
int  log_push(const uint8_t *buf, size_t len);

/* flushes log file, closes it and shutdowns log flushing thread */
void log_queisce();

#endif
