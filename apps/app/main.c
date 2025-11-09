#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/event.h>
#include <mach-o/getsect.h>

#include <app/app.h>
#include <io.h>
#include <seq.h>
#include <log.h>
#include <tty.h>
#include <term.h>
#include <misc.h>
#include <const.h>
#include <event.h>
#include <iboot.h>
#include <polina.h>
#include <lolcat.h>
#include <compiler.h>

#define DEFAULT_DRIVER      "serial"

#define APP_ARGUMENTS   ":nkilghu:r"

static char opts[64] = { 0 };

static void app_config_init(const char *driver_opts) {
    if (*driver_opts == ':') {
        driver_opts++;
    }

    snprintf(opts, sizeof(opts), "%s%s", APP_ARGUMENTS, driver_opts);
}

static void app_config_load_default(polina_config_t *config) {
    /* filters */
    config->filter_return = false;
    config->filter_delete = false;
    config->filter_iboot = false;
    config->filter_lolcat = false;

    /* state */
    config->delay = 0;
    config->retry = false;

    /* misc */
    config->logging_disabled = false;
}

static int app_config_load(int argc, const char *argv[], polina_config_t *config) {
    app_config_load_default(config);

    opterr = 0;
    optind = 1;
    optreset = 1;

    char c;
    while ((c = getopt(argc, (char *const *)argv, opts)) != -1) {
        switch (c) {
            case 'n': {
                config->filter_return = true;
                break;
            }

            case 'k': {
                config->filter_delete = true;
                break;
            }

            case 'i': {
                config->filter_iboot = true;
                break;
            }

            case 'u': {
                uint64_t tmp = 0;
                if (parse_numeric_arg(optarg, 10, &tmp, 0, UINT64_MAX) != 0) {
                    POLINA_ERROR("invalid input delay period");
                    return -1;
                }

                config->delay = (useconds_t)tmp;
                break;
            }

            case 'l': {
                config->filter_lolcat = true;
                break;
            }

            case 'g': {
                config->logging_disabled = true;
                break;
            }

            case 'r': {
                config->retry = true;
                break;
            }

            case 'h': {
                return -1;
            }

            case ':': {
                POLINA_WARNING("-%c needs argument", optopt);
                return -1;
            }

            case '?': {
                POLINA_WARNING("unknown argument -%c", optopt);
                return -1;
            }

            default: {
                /* might be consumed by drivers */
                break;
            }
        }
    }

    return 0;
}

static void app_print_cfg_internal(polina_config_t *config) {
    POLINA_INFO_NO_BREAK("return: ");
    POLINA_MISC_NO_BREAK("%s", bool_on_off(config->filter_return));

    POLINA_INFO_NO_BREAK(" delete: ");
    POLINA_MISC_NO_BREAK("%s", bool_on_off(config->filter_delete));

    POLINA_INFO_NO_BREAK(" iBoot: ");
    POLINA_MISC_NO_BREAK("%s", bool_on_off(config->filter_iboot));

    POLINA_INFO_NO_BREAK(" \x1B[38;5;154ml\x1B[39m\x1B[38;5;214mo\x1B[39m\x1B[38;5;198ml\x1B[39m\x1B[38;5;164mc\x1B[39m\x1B[38;5;63ma\x1B[39m\x1B[38;5;39mt\x1B[39m\x1B[38;5;49m\x1B[39m: ");
    POLINA_MISC_NO_BREAK("%s", bool_on_off(config->filter_lolcat));

    POLINA_LINE_BREAK();

    POLINA_INFO_NO_BREAK("delay: ");
    POLINA_MISC_NO_BREAK("%d", config->delay);

    POLINA_INFO_NO_BREAK(" logging: ");
    POLINA_MISC_NO_BREAK("%s", bool_on_off(!config->logging_disabled));

    POLINA_INFO_NO_BREAK(" reconnect: ");
    POLINA_MISC_NO_BREAK("%s", bool_on_off(config->retry));

    POLINA_LINE_BREAK();
}

app_arg_consumed_t app_config_arg_consumed(char c) {
    const char *__opts = APP_ARGUMENTS;
    char __opt = 0;

    while ((__opt = *__opts++)) {
        if (__opt == c) {
            if (*__opts == ':') {
                return APP_ARG_CONSUMED_WITH_ARG;
            }

            return APP_ARG_CONSUMED;
        }
    }

    return APP_ARG_NOT_CONSUMED;
}

static polina_config_t config = { 0 };

static struct {
    const driver_t *driver;
    event_t event;
} ctx = { 0 };

static int app_get_driver(const driver_t **driver, int *argc, const char ***argv) {
    const char *driver_name = DEFAULT_DRIVER;

    if (*argc == 1) {
        goto find;
    } else {
        const char *argv1 = (*argv)[1];
        if (argv1[0] != '-') {
            driver_name = argv1;
            (*argv)++;
            (*argc)--;
        }
    }

find:
    DRIVER_ITERATE({
        if (strcmp(driver_name, curr->name) == 0) {
            *driver = curr;
            break;
        }
    });

    if (!*driver) {
        POLINA_ERROR("driver \"%s\" is not known", driver_name);
        return -1;
    }

    return 0;
}

int app_event_signal(app_event_t event) {
    event_signal(&ctx.event, event);
    return 0;
}

static
void app_sigterm_handler(int s) {
    app_event_signal(APP_EVENT_DISCONNECT_SYSTEM);
}

static app_event_t app_event_loop() {
    /* set up SIGTERM handler - good when OS wants us quit */
    signal(SIGTERM, app_sigterm_handler);

    app_event_t event = event_wait(&ctx.event);

    /*
     * reset all lolcat color modes, otherwise messages below
     * will have a color of the last character from serial output
     */
    if (config.filter_lolcat) {
        lolcat_reset();
    }

    POLINA_LINE_BREAK();

    switch (event) {
        case APP_EVENT_DISCONNECT_SYSTEM: {
            POLINA_INFO("[disconnected - OS request]");
            break;
        }

        case APP_EVENT_DISCONNECT_DEVICE: {
            POLINA_INFO("[disconnected - device disappeared]");
            break;
        }

        case APP_EVENT_DISCONNECT_USER: {
            POLINA_INFO("[disconnected]");
            break;
        }

        case APP_EVENT_ERROR: {
            POLINA_ERROR("[internal error]");
            break;
        }

        default:
            break;
    }

    event_unsignal(&ctx.event);

    return event;
}

static int app_handle_user_input_cb(uint8_t c) {
    app_event_t event = APP_EVENT_NONE;

    if (c == 0x1D /* Ctrl+] */) {
        event = APP_EVENT_DISCONNECT_USER;
        goto err;
    }

    if (ctx.driver->write(&c, sizeof(c)) != 0) {
        POLINA_ERROR("couldn't write into device?!");
        event = APP_EVENT_ERROR;
        goto err;
    }

    return 0;

err:
    app_event_signal(event);
    return -1;
}

static int app_conn_cb() {
    POLINA_INFO("\n[connected - press CTRL+] to exit]");
    return 0;
}

static int app_restart_cb() {
    POLINA_INFO("[reconnected]\n");
    return 0;
}

int app_quiesce(int ret) {
    /* shutting down the driver */
    ctx.driver->quiesce();

    /* shutting down user input handling */
    io_quiesce();

    /* shutting down logging subsystem - this can take a bit of time */
    if (!config.logging_disabled) {
        log_queisce();
    }

    /* restore terminal config back to original state */
    if (term_restore_attrs() != 0) {
        POLINA_ERROR("could NOT restore terminal attributes - you might want to kill it");
        ret = -1;
    }

    /* free aux iBoot HMACs if needed */
    if (config.filter_iboot) {
        iboot_destroy_aux_hmacs();
    }

    return ret;
}

void app_print_cfg() {
    ctx.driver->print_cfg();
    app_print_cfg_internal(&config);
}

char __build_tag[256] = { 0 };

/* get our build tag from a separate Mach-O section */
static char *__get_tag() {
    if (*__build_tag) {
        return __build_tag;
    }

    unsigned long size = 0;
    char *data = (char *)getsectiondata(dlsym(RTLD_MAIN_ONLY, "_mh_execute_header"), "__TEXT", "__polina_tag", &size);
    if (!data) {
        POLINA_WARNING("couldn't get embedded build tag");
        data = PRODUCT_NAME "-???";
        size = sizeof(PRODUCT_NAME "-???");
    }

    strncpy(__build_tag, data, size);

    return __build_tag;
}

__noreturn
void __panic_terminate_hook() {
    app_quiesce(-1);
    abort();
}

void app_version() {
    POLINA_INFO("%s", __get_tag());
    POLINA_INFO("made by john (@nyan_satan)");
    POLINA_LINE_BREAK();
}

static void help(const char *program_name) {
    app_version();

    printf("usage: %s DRIVER <options>\n", program_name);
    printf("\n");

    DRIVER_ITERATE({
        POLINA_INFO_NO_BREAK("%s", curr->name);
        printf(":\n");
        curr->help();
        printf("\n");
    });

    printf("available filter options:\n");
    printf("\t-n\tadd \\r to lines without it, good for diags/probe debug logs/etc.\n");
    printf("\t-k\treplace delete keys (0x7F) with backspace (0x08), good for diags\n");
    printf("\t-i\ttry to identify filenames in obfuscated iBoot output\n");
    printf("\n");

    printf("available miscallenous options:\n");
    printf("\t-r\ttry to reconnect in case of connection loss\n");
    printf("\t-u <usecs>\tdelay period in microseconds after each inputted character,\n");
    printf("\t\t\tdefault - 0 (no delay), 20000 is good for XNU\n");
    printf("\t-l\tlolcat the output, good for screenshots\n");
    printf("\t-g\tdisable logging\n");
    printf("\t-h\tshow this help menu\n");
    printf("\n");

    printf("additional iBoot HMAC map can be loaded via POLINASERIAL_IBOOT_HMACS variable,\n");
    printf("it must be a path to a text file with the following structure:\n");
    printf("\n");
    printf("\tHMAC:FILENAME\n");
    printf("\tHMAC:FILENAME\n");
    printf("\t...\n");
    printf("\n");
    printf("check \"iboot_aux_hmacs.txt\" provided along with the program for reference\n");
    printf("\n");

    printf("default DRIVER is \"" DEFAULT_DRIVER "\"\n");
    printf("logs are collected to ~/Library/Logs/" PRODUCT_NAME "/\n");
}

int main(int argc, const char *argv[]) {
    POLINA_SET_THREAD_NAME("main");

    int ret = -1;

    /* getting driver & advancing args if needed */
    if (app_get_driver(&ctx.driver, &argc, &argv) != 0) {
        return -1;
    }

    /* make argument parser aware of driver-specific options */
    app_config_init(ctx.driver->optstring);

    /* load app configuration from args */
    if (app_config_load(argc, argv, &config) != 0) {
        help(argv[0]);
        return -1;
    }

    /* load additional iBoot HMACs from external file */
    if (config.filter_iboot && getenv(IBOOT_HMACS_VAR)) {
        if (iboot_load_aux_hmacs(getenv(IBOOT_HMACS_VAR)) != 0) {
            return -1;
        }
    }

    /* initialize selected driver */
    REQUIRE_NOERR(ctx.driver->init(argc, argv), out);

    /* check for unaccounted args */
    if (optind != argc) {
        help(argv[0]);
        return -1;
    }

    /* make IO subsystem aware of selected options */
    io_set_config(&config);

    /* scroll terminal to a new page and clear it */
    term_scroll();
    term_clear_page();

    /* print version */
    app_version();

    /* print full config - of both driver and the app itself */
    app_print_cfg();

    /* save TTY's configuration to restore it later */
    REQUIRE_NOERR(term_save_attrs(), out);

    /* set terminal to raw mode */
    REQUIRE_NOERR(term_set_raw(config.filter_return), out);

    /* driver preflight - menu and etc. */
    REQUIRE_NOERR(ctx.driver->preflight(), out);

    /* initialize logging subsystem if needed */
    if (!config.logging_disabled) {
        char dev_name[PATH_MAX + 1] = { 0 };
        ctx.driver->log_name(dev_name, sizeof(dev_name));

        REQUIRE_NOERR(log_init(dev_name), out);
    }

    /* initialize lolcat if needed */
    if (config.filter_lolcat) {
        lolcat_init();
    }

    /* initialize app event struct */
    event_init(&ctx.event);

    /* starting the driver */
    REQUIRE_NOERR(ctx.driver->start(io_out_cb, app_conn_cb), out);

    /* create user input handler thread */
    io_set_input_cb(app_handle_user_input_cb);
    io_user_input_start();

    app_event_t event;
    while (true) {
        /* this will block until we disconnect or something errors out */
        event = app_event_loop();

        if (config.retry && event == APP_EVENT_DISCONNECT_DEVICE) {
            POLINA_WARNING("[trying to reconnect, press CTRL+] to exit]");
            REQUIRE_NOERR(ctx.driver->restart(app_restart_cb), out);
        } else {
            break;
        }
    }

    ret = event == APP_EVENT_ERROR ? -1 : 0;

out:
    return app_quiesce(ret);
}
