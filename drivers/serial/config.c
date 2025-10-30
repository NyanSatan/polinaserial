#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <app/app.h>
#include <misc.h>

#include "config.h"
#include "baudrate_presets.h"

#define BAUDRATE_MIN    300
#define BAUDRATE_MAX    3000000

#define DATA_BITS_MIN   5
#define DATA_BITS_MAX   8

#define STOP_BITS_MIN   1
#define STOP_BITS_MAX   2

static int32_t find_baudrate_from_preset(const char *arg) {
    for (size_t i = 0; i < BAUDRATE_PRESET_COUNT; i++) {
        const struct baudrate_preset *curr = &baudrate_presets[i];
        if (strcmp(curr->name, arg) == 0) {
            return curr->baudrate;
        }
    }

    return -1;
}

int serial_config_load(int argc, const char *argv[], serial_config_t *config) {
    const char *help_reason = NULL;

    opterr = 0;
    optind = 1;
    optreset = 1;

    config->device = NULL;
    config->baudrate = 115200;
    config->data_bits = 8;
    config->stop_bits = 1;
    config->parity = PARITY_NONE;
    config->flow_control = FLOW_CONTROL_NONE;

    char c;
    while ((c = getopt(argc, (char *const *)argv, SERIAL_ARGUMENTS)) != -1) {
        app_arg_consumed_t consumed = app_config_arg_consumed(optopt);
        
        switch (consumed) {
            case APP_ARG_CONSUMED_WITH_ARG:
                optind++;
            case APP_ARG_CONSUMED:
                continue;

            default:
                break;
        }

        switch (c) {
            case 'd': {
                config->device = optarg;
                break;
            }

            case 'b': {
                int32_t baudrate_preset = find_baudrate_from_preset(optarg);

                if (baudrate_preset != -1) {
                    config->baudrate = baudrate_preset;
                } else {
                    uint64_t baudrate = 0;
                    if (parse_numeric_arg(optarg, 10, &baudrate, BAUDRATE_MIN, BAUDRATE_MAX) != 0) {
                        help_reason = "baudrate must be a decimal number not smaller than " STR(BAUDRATE_MIN) " and not bigger than " STR(BAUDRATE_MAX);
                        goto help_needed;
                    }

                    config->baudrate = (uint32_t)baudrate;
                }

                break;
            }

#if WITH_UART_EXTRA
            case 'c': {
                uint64_t data_bits;

                if (parse_numeric_arg(optarg, 10, &data_bits, DATA_BITS_MIN, DATA_BITS_MAX) != 0){
                    help_reason = "data bits must be a decimal number not smaller than " STR(DATA_BITS_MIN) " and not bigger than " STR(DATA_BITS_MAX);
                    goto help_needed;
                }

                config->data_bits = (uint32_t)data_bits;

                break;
            }

            case 't': {
                uint64_t stop_bits;

                if (parse_numeric_arg(optarg, 10, &stop_bits, STOP_BITS_MIN, STOP_BITS_MAX) != 0){
                    help_reason = "stop bits must be a decimal number not smaller than " STR(STOP_BITS_MIN) " and not bigger than " STR(STOP_BITS_MAX);
                    goto help_needed;
                }

                config->stop_bits = (uint32_t)stop_bits;

                break;
            }

            case 'p': {
                if (strcmp(optarg, "none") == 0) {
                    config->parity = PARITY_NONE;
                } else if (strcmp(optarg, "odd") == 0) {
                    config->parity = PARITY_ODD;
                } else if (strcmp(optarg, "even") == 0) {
                    config->parity = PARITY_EVEN;
                } else {
                    help_reason = "parity must be either none or odd or even";
                    goto help_needed;
                }

                break;
            }

            case 'f': {
                if (strcmp(optarg, "none") == 0) {
                    config->flow_control = FLOW_CONTROL_NONE;
                } else if (strcmp(optarg, "sw") == 0) {
                    config->flow_control = FLOW_CONTROL_SW;
                } else if (strcmp(optarg, "hw") == 0) {
                    config->flow_control = FLOW_CONTROL_HW;
                } else {
                    help_reason = "flow control must be either none or sw or hw";
                    goto help_needed;
                }

                break;
            }
#endif
            case ':': {
                POLINA_WARNING("-%c needs argument", optopt);
                return -1;
            }

            default:
                abort();
        }
    }

    return 0;

help_needed:
    if (help_reason) {
        POLINA_WARNING("%s", help_reason);
    }

    return -1;
}

void serial_print_cfg(serial_config_t *config) {
    POLINA_INFO_NO_BREAK("device: ");
    if (config->device) {
        POLINA_MISC_NO_BREAK("%s", config->device);
    } else {
        POLINA_MISC_NO_BREAK("menu");
    }

    POLINA_INFO_NO_BREAK(" baud: ");
    POLINA_MISC_NO_BREAK("%d", config->baudrate);

    POLINA_INFO_NO_BREAK(" data: ");
    POLINA_MISC_NO_BREAK("%d", config->data_bits);

    POLINA_INFO_NO_BREAK(" stop: ");
    POLINA_MISC_NO_BREAK("%d", config->stop_bits);

    POLINA_INFO_NO_BREAK(" parity: ");
    switch (config->parity) {
        case PARITY_NONE:
            POLINA_MISC_NO_BREAK("none");
            break;

        case PARITY_EVEN:
            POLINA_MISC_NO_BREAK("even");
            break;

        case PARITY_ODD:
            POLINA_MISC_NO_BREAK("odd");
            break;
    }

    POLINA_INFO_NO_BREAK(" flow: ");
    switch (config->flow_control) {
        case FLOW_CONTROL_NONE:
            POLINA_MISC_NO_BREAK("none");
            break;

        case FLOW_CONTROL_HW:
            POLINA_MISC_NO_BREAK("hw");
            break;

        case FLOW_CONTROL_SW:
            POLINA_MISC_NO_BREAK("sw");
            break;
    }

    POLINA_LINE_BREAK();
}

void serial_help() {
    printf("\t-d <device>\tpath to device (default - shows menu)\n");
    printf("\t-b <baudrate>\tbaudrate to use (default - ios/115200)\n");
    POLINA_LINE_BREAK();
    printf("\tavailable baudrate presets:\n");

    for (int i = 0; i < BAUDRATE_PRESET_COUNT; i++) {
        const struct baudrate_preset *curr = &baudrate_presets[i];
        printf("\t\t%-12.12s- %s (%d)\n", curr->name, curr->description, curr->baudrate);
    }

    POLINA_LINE_BREAK();

#if WITH_UART_EXTRA
    printf("\t-c <bits>\tdata bits - from 5 to 8 (default - 8)\n");
    printf("\t-t <bits>\tstop bits - 1 or 2 (default - 1)\n");
    printf("\t-p <parity>\tparity - none, even or odd (default - none)\n");
    printf("\t-f <control>\tflow control - none, sw or hw (default - none)\n");
#else
    printf("\tthe rest will default to 8N1 (no flow control),\n");
    printf("\tas the author of this tool cannot currently test other configurations\n");
#endif
}
