#ifndef POLINA_HALT_H
#define POLINA_HALT_H

void _panic_print(const char *file, const char *func, int line, const char *fmt, ...);
__attribute__((noreturn)) void _panic_terminate();

/*
 * call panic() (printf-like) in case of an unexpected & fatal error,
 * it will print a traceback & other infos for debug
 */
#define panic(x...) \
    _panic_print(__FILE__, __FUNCTION__, __LINE__, x); _panic_terminate();

#endif
