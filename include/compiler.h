#ifndef POLINA_COMPILER_H
#define POLINA_COMPILER_H

#define __noreturn \
    __attribute__((noreturn))

#define unlikely(x) \
    __builtin_expect(!!(x), 0)

#endif
