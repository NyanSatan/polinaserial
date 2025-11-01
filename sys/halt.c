#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <execinfo.h>
#include <misc.h>

extern char __build_tag[];

void _panic_print(const char *file, const char *func, int line, const char *fmt, ...) {
    char panic_msg[1024]  = { 0 };
    char thread_name[128] = { 0 };
    void *callstack[128]  = { 0 };

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(panic_msg, sizeof(panic_msg), fmt, ap);
    va_end(ap);

    POLINA_ERROR("\n\n[polinaserial panic]: %s\n", panic_msg);

    POLINA_ERROR("tag:\t%s", __build_tag);
    POLINA_ERROR("file:\t%s:%d", file, line);
    POLINA_ERROR("func:\t%s()", func);

    pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
    int frames = backtrace(callstack, 128);
    char **strs = backtrace_symbols(callstack, frames);

    POLINA_ERROR("\nthread '%s' backtrace:", thread_name);
    for (int i = 0; i < frames; i++) {
        POLINA_ERROR("%s", strs[i]);
    }

    POLINA_ERROR("\nsomething truly terrible has happened, please report this panic");
}
