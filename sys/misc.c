#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

int parse_numeric_arg(const char *arg, int base, uint64_t *val, uint64_t min_val, uint64_t max_val) {
    char *stop;
    uint64_t result = strtoull(arg, &stop, base);
    if (*stop || result > max_val || result < min_val) {
        return -1;
    }
    
    *val = result;
    
    return 0;
}

const char *bool_on_off(bool status) {
    return status ? "on" : "off";
}

const char *last_path_component(const char *path) {
    const char *ptr = strrchr(path, '/');
    if (ptr) {
        return ptr + 1;
    } else {
        return path;
    }
}

int mkdir_recursive(const char *path) {
    char temp[PATH_MAX + 1];
    char *curr = (char *)&temp;

    if (strlcpy(temp, path, sizeof(temp)) >= sizeof(temp)) {
        return -1;
    }

    while (1) {
        char *slash = strchr(curr, '/');

        if (slash) {
            if (slash == curr) {
                curr++;
                continue;
            }

            *slash = '\0';
        }

        if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
            return -1;
        }

        if (slash) {
            *slash = '/';
            curr = slash + 1;
        } else {
            return 0;
        }
    }
}

char *itoa(int i, char *a, size_t l) {
    a[--l] = '\0';

    do {
        int curr = i % 10;
        a[l--] = '0' + curr;
        i /= 10;
    } while (i && l);

    return a + l;
}
