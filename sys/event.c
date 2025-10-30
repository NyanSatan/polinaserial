#include <event.h>

void event_init(event_t *event) {
    pthread_mutex_init(&event->mutex, NULL);
    pthread_cond_init(&event->cond, NULL);
    event->value = 0;
}

void event_signal(event_t *event, uint64_t val) {
    pthread_mutex_lock(&event->mutex);
    event->value = val;
    pthread_cond_signal(&event->cond);  
    pthread_mutex_unlock(&event->mutex);
}

void event_unsignal(event_t *event) {
    event->value = 0;
    pthread_mutex_unlock(&event->mutex);
}

uint64_t event_wait(event_t *event) {
    pthread_mutex_lock(&event->mutex);
    while (event->value == 0) {
        pthread_cond_wait(&event->cond, &event->mutex);
    }

    return event->value;
}
