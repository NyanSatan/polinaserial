#ifndef POLINA_EVENT_H
#define POLINA_EVENT_H

#include <stdint.h>
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint64_t value;
} event_t;

void event_init(event_t *event);
void event_signal(event_t *event, uint64_t val);
void event_unsignal(event_t *event);
uint64_t event_wait(event_t *event);

#endif
