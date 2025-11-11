/*
 * Boilerplate code for using Polina as a library,
 * e.g. in Virtual iBoot Fun
 */

#include <sys/event.h>
#include <string.h>

#include <io.h>
#include <log.h>
#include <misc.h>
#include <term.h>
#include <const.h>
#include <thread.h>
#include <iboot.h>
#include <lolcat.h>
#include <polina.h>
#include <compiler.h>

char __build_tag[] = BUILD_TAG;

static polina_config_t *cfg = NULL;

int polina_init(polina_config_t *_cfg, const char *log_name) {
    int ret = -1;

    cfg = _cfg;

    if (!cfg->logging_disabled) {
        REQUIRE_NOERR(log_init(log_name), out);
    }

    if (cfg->filter_lolcat) {
        lolcat_init();
    }

    if (cfg->filter_iboot && getenv(IBOOT_HMACS_VAR)) {
        REQUIRE_NOERR(iboot_load_aux_hmacs(getenv(IBOOT_HMACS_VAR)), out);
    }

    // XXX move this to polina_start_io()?
    REQUIRE_NOERR(term_save_attrs(), out);
    REQUIRE_NOERR(term_set_raw(cfg->filter_return), out);

    ret = 0;

out:
    if (ret != 0) {
        polina_quiesce();
    }

    return ret;
}

static int output_kq = -1;
static pthread_t output_thr = NULL;

static void *output_thr_handler(void *arg) {
    POLINA_SET_THREAD_NAME("polinalib output loop");

    uint8_t buf[IO_MAX_BUFFER_SIZE];

    int out_fd = (int)(uint64_t)arg;

    struct kevent ke = { 0 };

    output_kq = kqueue();
    if (output_kq == -1) {
        POLINA_ERROR("kqueue() failed");
        goto out;
    }

    thread_add_shutdown_ke(output_kq);

    EV_SET(&ke, out_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(output_kq, &ke, 1, NULL, 0, NULL);

    while (true) {
        memset(&ke, 0, sizeof(ke));
        int i = kevent(output_kq, NULL, 0, &ke, 1, NULL);

        if (i == 0) {
            continue;
        }

        if (thread_check_shutdown_ke(&ke)) {
            goto out;
        }

        ssize_t r = read(out_fd, buf, sizeof(buf));

        if (r > 0) {
            if (io_out_cb(buf, r) != 0) {
                goto out;
            }
        }
    }

out:
    close(output_kq);
    output_kq = -1;

    return NULL;
}

int polina_start_io(int (*in_cb)(uint8_t c), int out) {
    io_set_config(cfg);

    io_set_input_cb(in_cb);
    io_user_input_start();

    pthread_create(&output_thr, NULL, output_thr_handler, (void *)(uint64_t)out);

    return 0;
}

void polina_quiesce() {
    thread_trigger_shutdown_ke(&output_thr, &output_kq);

    io_quiesce();

    if (cfg && !cfg->logging_disabled) {
        log_queisce();
    }

    if (term_restore_attrs() != 0) {
        POLINA_ERROR("could NOT restore terminal attributes - you might want to kill it");
    }

    if (cfg && cfg->filter_iboot) {
        iboot_destroy_aux_hmacs();
    }
}

__noreturn
void __panic_terminate_hook() {
    POLINA_ERROR("polina crashed, very bad!");
    polina_quiesce();

    while (1) {
        sleep(1);
    }
}
