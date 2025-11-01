#include <string.h>
#include <sys/event.h>

#include <io.h>
#include <seq.h>
#include <log.h>
#include <misc.h>
#include <iboot.h>
#include <lolcat.h>
#include <polina.h>


static polina_config_t *cfg = NULL;
static int (*user_input_cb)(uint8_t c) = NULL;
static pthread_t user_input_thr = { 0 };

/* gotta have this buffer much larger than driver's because of lolcat and etc. */
static uint8_t out_buf[IO_MAX_BUFFER_SIZE * 16] = { 0 };
static seq_ctx_t seq_ctx = { 0 };

void io_set_config(polina_config_t *_cfg) {
    cfg = _cfg;
}

void io_set_input_cb(int (*_cb)(uint8_t c)) {
    user_input_cb = _cb;
}

int io_out_cb(uint8_t *in_buf, size_t in_len) {
    size_t out_len = 0;
    uint8_t *curr_buf = in_buf;
    size_t left = in_len;

    /* call seq_process_chars() until the packet ends */
    while (left) {
        size_t _out_len = 0;
        lolcat_handler_t lolcatify = NULL;
        iboot_log_line_t iboot_line = { 0 };

        /* returns amount of bytes of the same sequence type until it breaks by a different one */
        int cnt = seq_process_chars(&seq_ctx, curr_buf, left);
        REQUIRE_PANIC(cnt > 0);

        if (cnt > left) {
            panic("too many bytes (cnt: %d, left: %d)", cnt, left);
        }

        switch (seq_ctx.type) {
            /* printable ASCII, just lolcatify if requested */
            case kSeqNormal: {
                if (cfg->filter_lolcat) {
                    lolcatify = lolcat_push_ascii;
                }

                break;
            }

            /*
             * UTF-8, lolcatify, but ONLY if it starts with first UTF-8 byte,
             * inserting ANSI sequences between UTF-8 char bytes will break it
             */
            case kSeqUnicode: {
                if (cfg->filter_lolcat) {
                    if (seq_ctx.has_utf8_first_byte) {
                        lolcatify = lolcat_push_one;
                    }
                }

                break;
            }

            /* control characters, CSI sequences and unrecognized stuff don't get any special handling */
            case kSeqControl:
            case kSeqEscapeCSI:
            case kSeqUnknown:
                break;

            default:
                panic("the hell is this sequence (%d)", seq_ctx.type);
        }

        /* feed current ASCII output into iBoot unobfuscator state machine */
        if (cfg->filter_iboot && (seq_ctx.type == kSeqNormal)) {
            REQUIRE_PANIC_NOERR(iboot_push_data(curr_buf, cnt));
        }

        /*
         * iBoot always prints carriage return (\r) before new line (\n),
         * good moment for us to ask the state machine if it found something,
         * and if yes, we print it
         */
        if (seq_ctx.type == kSeqControl && *curr_buf == '\r') {
            if (iboot_trigger(&iboot_line)) {
                _out_len = sizeof(out_buf) - out_len;
                REQUIRE_PANIC_NOERR(iboot_output_file(&iboot_line, out_buf + out_len, &_out_len));
                out_len += _out_len;

                if (cfg->filter_lolcat) {
                    lolcat_refresh();
                }
            }
        }

        if (lolcatify) {
            _out_len = sizeof(out_buf) - out_len;
            REQUIRE_PANIC_NOERR(lolcatify(curr_buf, cnt, out_buf + out_len, &_out_len));
            out_len += _out_len;
        } else {
            memcpy(out_buf + out_len, curr_buf, cnt);
            out_len += cnt;
        }

        if (!cfg->logging_disabled) {
            switch (seq_ctx.type) {
                /* do not push control sequences into log file */
                case kSeqNormal:
                case kSeqUnicode:
                    REQUIRE_PANIC_NOERR(log_push(curr_buf, cnt));

                default:
                    break;
            }
        }

        curr_buf += cnt;
        left -= cnt;
    }

    /* packet processing done, write into terminal at last */
    write(STDOUT_FILENO, out_buf, out_len);

    return 0;
}

static void *io_user_input_thread(void *arg) {
    POLINA_SET_THREAD_NAME("user input loop");

    struct kevent ke = { 0 };

    int kq = kqueue();
    if (kq == -1) {
        POLINA_ERROR("kqueue() failed");
        goto out;
    }

    EV_SET(&ke, STDIN_FILENO, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &ke, 1, NULL, 0, NULL);

    char c = 0;
    while (true) {
        memset(&ke, 0, sizeof(ke));
        int i = kevent(kq, NULL, 0, &ke, 1, NULL);

        if (i == 0) {
            continue;
        }

        ssize_t r = read(STDIN_FILENO, &c, 1);

        if (cfg->filter_delete && c == 0x7F) {
            c = 0x08;
        }

        if (user_input_cb) {
            if (user_input_cb(c) != 0) {
                goto out;
            }

            if (cfg->delay) {
                usleep(cfg->delay);
            }
        }
    }

out:
    close(kq);

    return NULL;
}

void io_user_input_start() {
    pthread_create(&user_input_thr, NULL, io_user_input_thread, NULL);
}
