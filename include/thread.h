/* thread shutdown helpers */

#ifndef POLINA_THREAD_H
#define POLINA_THREAD_H

#include <stdbool.h>
#include <pthread.h>

void thread_add_shutdown_ke(int kq);
bool thread_check_shutdown_ke(struct kevent *ke);
void thread_trigger_shutdown_ke(pthread_t *thr, int *kq);

#endif
