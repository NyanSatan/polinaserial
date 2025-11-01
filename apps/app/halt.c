#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <execinfo.h>
#include <app/app.h>
#include <misc.h>

extern char __build_tag[];

__attribute__((noreturn))
void _panic_terminate() {
    app_quiesce(-1);
    abort();
}
