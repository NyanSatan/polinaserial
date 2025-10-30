#ifndef POLINA_MISC_H
#define POLINA_MISC_H

/*
 * Various useful macroses & functions
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <halt.h>

#define REQUIRE(_cond, _label) \
    if (!(_cond)) { \
        goto _label; \
    }

#define REQUIRE_NOERR(_expr, _label) \
    if (_expr != 0) { \
        goto _label; \
    }

#define REQUIRE_PANIC(_cond) \
    if (!(_cond)) { \
        panic("REQUIRE failed - " #_cond); \
    }

#define REQUIRE_PANIC_NOERR(_expr) \
    if (_expr != 0) { \
        panic("REQUIRE_NOERR failed - " #_expr); \
    }

#define __STR_IMPL_(x)    #x
#define STR(x)          __STR_IMPL_(x)

#define ANSI_START   "\x1b["
#define ANSI_DELIM   ";"
#define ANSI_BOLD    "1"
#define ANSI_RED     "31"
#define ANSI_GREEN   "32"
#define ANSI_YELLOW  "33"
#define ANSI_RESET   "0"
#define ANSI_END     "m"

#define BREAK   "\r\n"

#define POLINA_LINE_BREAK()                    printf(BREAK);

#define POLINA_MISC(__format, ...)             printf(__format BREAK, ##__VA_ARGS__);
#define POLINA_MISC_NO_BREAK(__format, ...)    printf(__format, ##__VA_ARGS__);

#define POLINA_INFO(__format, ...)             printf(ANSI_START ANSI_BOLD ANSI_END __format ANSI_START ANSI_RESET ANSI_END BREAK, ##__VA_ARGS__);
#define POLINA_INFO_NO_BREAK(__format, ...)    printf(ANSI_START ANSI_BOLD ANSI_END __format ANSI_START ANSI_RESET ANSI_END, ##__VA_ARGS__);

#define POLINA_SUCCESS(__format, ...)          printf(ANSI_START ANSI_GREEN ANSI_DELIM ANSI_BOLD ANSI_END __format ANSI_START ANSI_RESET ANSI_END BREAK, ##__VA_ARGS__);
#define POLINA_SUCCESS_NO_BREAK(__format, ...) printf(ANSI_START ANSI_GREEN ANSI_DELIM ANSI_BOLD ANSI_END __format ANSI_START ANSI_RESET ANSI_END, ##__VA_ARGS__);

#define POLINA_WARNING(__format, ...)          printf(ANSI_START ANSI_YELLOW ANSI_DELIM ANSI_BOLD ANSI_END __format ANSI_START ANSI_RESET ANSI_END BREAK, ##__VA_ARGS__);
#define POLINA_ERROR(__format, ...)            printf(ANSI_START ANSI_RED ANSI_DELIM ANSI_BOLD ANSI_END __format ANSI_START ANSI_RESET ANSI_END BREAK, ##__VA_ARGS__);

#define CONST_STRLEN(_s)    (sizeof(_s) - 1)

int mkdir_recursive(const char *path);
int parse_numeric_arg(const char *arg, int base, uint64_t *val, uint64_t min_val, uint64_t max_val);

const char *bool_on_off(bool status);
const char *last_path_component(const char *path);

char *itoa(int i, char *a, size_t l);

#define POLINA_SET_THREAD_NAME(__name) \
    pthread_setname_np("polina " __name)

#endif
