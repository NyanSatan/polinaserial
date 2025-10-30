/*
 * iBoot obfuscated logs' HMAC matching state machine of sorts,
 * kinda ugly one
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include <misc.h>

#include <iboot.h>
#include "iboot_config.h"

static iboot_hmac_config_t *aux_iboot_hmac_config = NULL;
static size_t aux_iboot_hmac_config_count = 0;

typedef enum {
    STATE_WAITING_FOR_HMAC = 0,
    STATE_WAITING_FOR_LINE,
} iboot_hmac_state_t;

static uint64_t current_hmac = 0;
static uint8_t hmac_digit_count = 0;

static uint32_t current_line = 0;
static uint8_t line_digit_count = 0;

static iboot_hmac_state_t state = STATE_WAITING_FOR_HMAC;

static void iboot_clear_state() {
    current_hmac = 0;
    hmac_digit_count = 0;

    current_line = 0;
    line_digit_count = 0;

    state = STATE_WAITING_FOR_HMAC;
}

/* any lowercase hex digit */
static int8_t iboot_hmac_character(char c) {
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 0xA;
    } else if (c >= '0' && c <= '9') {
        return c - '0';
    } else {
        return -1;
    }
}

/* only decimal digits */
static int8_t iboot_line_character(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else {
        return -1;
    }
}

int iboot_push_data(const uint8_t *data, size_t data_len) {
    for (size_t i = 0; i < data_len; i++) {
        uint8_t c = data[i];

        switch (state) {
            case STATE_WAITING_FOR_HMAC: {
                int8_t hex_digit = iboot_hmac_character(c);

                if (hex_digit != -1) {
                    if (hmac_digit_count == 64 / 4) {
                        iboot_clear_state();
                        break;
                    }

                    current_hmac |= (uint64_t)hex_digit << ((64 - hmac_digit_count * 4) - 4);

                    hmac_digit_count++;

                } else {
                    if (c == ':' && hmac_digit_count > 0) {
                        if (hmac_digit_count != 64 / 4) {
                            current_hmac >>= (64 - hmac_digit_count * 4);
                        }

                        state = STATE_WAITING_FOR_LINE;
                    } else {
                        iboot_clear_state();
                    }
                }

                break;
            }

            case STATE_WAITING_FOR_LINE: {
                int8_t line_digit = iboot_line_character(c);

                if (line_digit != -1) {
                    if (line_digit_count == 6) {
                        iboot_clear_state();
                        break;
                    }

                    current_line = current_line * 10 + line_digit;

                } else {
                    iboot_clear_state();
                }

                break;
            }
        }
    }

out:
    return 0;
}

#define START_FILE  "\t" ANSI_START ANSI_GREEN ANSI_DELIM ANSI_BOLD ANSI_END "<"
#define END_FILE    ">" ANSI_START ANSI_RESET ANSI_END

int iboot_output_file(iboot_log_line_t *line, uint8_t *buf, size_t *out_len) {
    size_t max_len = *out_len;
    size_t curr_len = 0;
    char line_buf[8] = { 0 };
    char *file_line = NULL;

#define PUSH(_data, _len) \
    do { \
        if (curr_len + _len > max_len) { return -1; } \
        memcpy(buf + curr_len, _data, _len); \
        curr_len += _len; \
    } while(0)

    PUSH(START_FILE, CONST_STRLEN(START_FILE));
    PUSH(line->file, strlen(line->file));
    PUSH(":", 1);
    file_line = itoa(line->line, line_buf, sizeof(line_buf));
    PUSH(file_line, sizeof(line_buf) - (file_line - line_buf));
    PUSH(END_FILE, CONST_STRLEN(END_FILE));

    *out_len = curr_len;

    return 0;
}


static const char *iboot_find_file_for_hmac_internal(iboot_hmac_config_t *config, size_t count, uint64_t hmac) {
    if (hmac < config[0].hmac || hmac > config[count - 1].hmac) {
        return NULL;
    }

    const iboot_hmac_config_t *current_config = (const iboot_hmac_config_t *)config;

    size_t l = 0;
    size_t r = count - 1;

    while (l <= r) {
        ssize_t c = (l + r) / 2;

        const iboot_hmac_config_t *current = (const iboot_hmac_config_t *)&current_config[c];

        if (current->hmac < hmac) { 
            l = c + 1;
        } else if (current->hmac > hmac) {
            r = c - 1;
        } else {
            return current->file;
        }
    }

    return NULL;
}

static const char *iboot_find_file_for_hmac(uint64_t hmac) {
    const char *res = iboot_find_file_for_hmac_internal((iboot_hmac_config_t *)iboot_hmac_config, iboot_hmac_config_count, hmac);
    if (res) {
        return res;
    } else if (aux_iboot_hmac_config && aux_iboot_hmac_config_count) {
        /* look up auxiliary database if it's loaded */
        return iboot_find_file_for_hmac_internal(aux_iboot_hmac_config, aux_iboot_hmac_config_count, hmac);
    }

    return NULL;
}

bool iboot_trigger(iboot_log_line_t *line) {
    bool ret = false;
    if (state == STATE_WAITING_FOR_LINE) {
        const char *file = iboot_find_file_for_hmac(current_hmac);
        if (file) {
            line->file = file;
            line->line = current_line;
            ret = true;
        }
    }

    iboot_clear_state();

    return ret;
}

// ============================ Aux HMACs loading ============================

static int _read_file(const char *path, char **buf, size_t *len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        POLINA_ERROR("failed to open aux iBoot HMAC database");
        return -1;
    }

    struct stat st = { 0 };
    fstat(fd, &st);
    size_t _len = st.st_size;

    char *_buf = malloc(_len + 1 /* final line break */ + 1 /* NULL terminator */);
    if (!_buf) {
        POLINA_ERROR("out of memory?!");
        close(fd);
        return -1;
    }

    int ret = read(fd, _buf, _len);
    close(fd);

    if (ret != _len) {
        POLINA_ERROR("failed to read aux iBoot HMAC database");
        free(_buf);
        return -1;
    }

    /* if there's no line break in the end - add it */
    if (_buf[_len - 1] != '\n') {
        _buf[_len] = '\n';
        _len++;
    }

    _buf[_len] = '\0';

    *buf = _buf;
    *len = _len;

    return 0;
}

#define HMAC_MAX_RAW_LEN    (16 /* digits */ + 2 /* "0x" */)
#define PATH_MAX_LEN        (PATH_MAX)

static int _parse_line(const char *buf, size_t len, iboot_hmac_config_t *out) {
    const char *delim = strchr(buf, ':');

    size_t hmac_raw_len = delim - buf;
    if (hmac_raw_len > HMAC_MAX_RAW_LEN) {
        POLINA_ERROR("HMAC is too long!");
        return -1;
    }

    char hmac_raw[HMAC_MAX_RAW_LEN + 1] = { 0 };
    memcpy(hmac_raw, buf, hmac_raw_len);

    char *endptr = NULL;
    uint64_t hmac = strtoull(hmac_raw, &endptr, 16);
    if (*endptr != '\0') {
        POLINA_ERROR("failed to decode HMAC!");
        return -1;
    }

    off_t name_start = delim - buf + 1;
    size_t name_len = len - name_start;

    if (name_len > PATH_MAX_LEN) {
        POLINA_ERROR("name is too long!");
        return -1;
    }

    out->hmac = hmac;
    out->file = strndup(buf + name_start, name_len);

    return 0;
}

static int _parse_file(const char *buf, size_t len, iboot_hmac_config_t out[], int *cnt) {
    int _cnt = 0;
    off_t idx = 0;

    while (idx < len) {
        const char *l_end = strchr(buf + idx, '\n');
        if (!l_end) {
            break;
        }

        size_t l_len = l_end - (buf + idx);

        if (l_len != 0 && buf[idx] != '#') {
            if (out) {
                if (_parse_line(buf + idx, l_len, &out[_cnt]) != 0) {
                    POLINA_ERROR("bad line @ %lld", idx);
                    return -1;
                }
            }

            _cnt++;
        }

        idx += l_len + 1;
    }

    *cnt = _cnt;

    return 0;
}

static int _comp_func(const void *one, const void *two) {
    uint64_t _one = ((iboot_hmac_config_t *)one)->hmac;
    uint64_t _two = ((iboot_hmac_config_t *)two)->hmac;

    if (_one < _two) {
        return -1;
    } else if (_one == _two) {
        return 0;
    } else if (_one > _two) {
        return 1;
    }

    abort();
}

int iboot_load_aux_hmacs(const char *path) {
    char *cont = NULL;
    size_t len = 0;

    if (_read_file(path, &cont, &len) != 0) {
        return -1;
    }

    int cnt = 0;
    if (_parse_file(cont, len, NULL, &cnt) != 0) {
        return -1;
    }

    iboot_hmac_config_t *_aux = malloc(cnt * sizeof(iboot_hmac_config_t));
    if (!_aux) {
        POLINA_WARNING("out of memory?!");
        return -1;
    }

    if (_parse_file(cont, len, _aux, &cnt) != 0) {
        return -1;
    }

    qsort(_aux, cnt, sizeof(*_aux), _comp_func);

    aux_iboot_hmac_config = _aux;
    aux_iboot_hmac_config_count = cnt;

    return 0;
}

void iboot_destroy_aux_hmacs() {
    if (aux_iboot_hmac_config && aux_iboot_hmac_config_count) {
        for (size_t i = 0; i < aux_iboot_hmac_config_count; i++) {
            free(aux_iboot_hmac_config[i].file);
        }

        free(aux_iboot_hmac_config);
    }

    aux_iboot_hmac_config = NULL;
    aux_iboot_hmac_config_count = 0;
}
