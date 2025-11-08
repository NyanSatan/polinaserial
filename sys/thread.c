#include <sys/event.h>
#include <thread.h>

#define DATA_LOOP_SHUTDOWN_ID   (613)

void thread_add_shutdown_ke(int kq) {
    struct kevent ke = { 0 };
    EV_SET(&ke, DATA_LOOP_SHUTDOWN_ID, EVFILT_USER, EV_ADD, 0, 0, NULL);
    kevent(kq, &ke, 1, NULL, 0, NULL);
}

bool thread_check_shutdown_ke(struct kevent *ke) {
    return (ke->filter == EVFILT_USER && ke->ident == DATA_LOOP_SHUTDOWN_ID);
}

void thread_trigger_shutdown_ke(pthread_t *thr, int *kq) {
    /* pottentially racy if the thread closes by itself, but I guess the APIs should just silently fail? */
    if (*thr && *kq != -1) {
        struct kevent ke = { 0 };
        EV_SET(&ke, DATA_LOOP_SHUTDOWN_ID, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
        kevent(*kq, &ke, 1, 0, 0, NULL);

        pthread_join(*thr, NULL);
        *thr = NULL;

        *kq = -1;
    }
}
