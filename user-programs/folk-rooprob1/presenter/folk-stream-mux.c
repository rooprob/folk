/*
 * folk-stream-mux — Stream multiplexer for Folk → ffmpeg pipeline
 *
 * Reads JPEG frames from N input FIFOs, selects one based on control
 * commands, and writes it to stdout at a target FPS. Falls back to a
 * built-in testcard when no source is active.
 *
 * Input FIFOs:  /tmp/folk-mux-{0..N}.fifo
 * Control pipe: /tmp/folk-mux-ctl.fifo  (text commands, one per line)
 * Output:       stdout (pipe to ffmpeg)
 *
 * Control commands:
 *   source N   — forward FIFO N
 *   testcard   — switch to testcard
 *   auto       — forward highest-index FIFO with recent data
 *
 * Usage:
 *   folk-stream-mux -n 2 -t testcard.jpg -f 10 | ffmpeg ...
 *
 * Build:
 *   gcc -O2 -o folk-stream-mux folk-stream-mux.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <stdint.h>
#include <getopt.h>

#define MAX_INPUTS       8
#define READ_BUF_SIZE    (2 * 1024 * 1024)
#define MAX_FRAME_SIZE   (1 * 1024 * 1024)
#define CTL_BUF_SIZE     4096

static volatile int running = 1;

typedef enum { MODE_AUTO, MODE_SOURCE, MODE_TESTCARD } MuxMode;

typedef struct {
    char   path[256];
    int    fd;
    /* Read buffer — accumulates raw bytes from FIFO */
    uint8_t *read_buf;
    size_t   read_used;
    /* Latest complete JPEG frame */
    uint8_t *frame;
    size_t   frame_size;
    struct timespec frame_time;
} Input;

static Input    inputs[MAX_INPUTS];
static int      num_inputs     = 0;
static int      ctl_fd         = -1;
static char     ctl_path[256]  = "/tmp/folk-mux-ctl.fifo";
static char     ctl_buf[CTL_BUF_SIZE];
static size_t   ctl_used       = 0;

static MuxMode  mode           = MODE_AUTO;
static int      selected       = -1;

static uint8_t *testcard       = NULL;
static size_t   testcard_size  = 0;

static int      target_fps     = 10;
static int      timeout_ms     = 500;

/* FPS tracking */
static int      out_frames     = 0;
static struct timespec fps_start;

static void on_signal(int s) { (void)s; running = 0; }

static double elapsed_ms(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1000.0 +
           (b->tv_nsec - a->tv_nsec) / 1e6;
}

/* ── Testcard ────────────────────────────────────────────────── */

static int load_testcard(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "mux: can't open testcard %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    testcard_size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    testcard = malloc(testcard_size);
    if (fread(testcard, 1, testcard_size, f) != testcard_size) {
        fclose(f); free(testcard); testcard = NULL; return 0;
    }
    fclose(f);
    fprintf(stderr, "mux: loaded testcard %s (%zu bytes)\n", path, testcard_size);
    return 1;
}

/* ── Pipe helpers ────────────────────────────────────────────── */

static void ensure_fifo(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        mkfifo(path, 0666);
    }
}

/* Open with O_RDWR to keep a write-ref → prevents POLLHUP spin */
static int open_input(Input *inp) {
    if (inp->fd >= 0) return 1;
    ensure_fifo(inp->path);
    inp->fd = open(inp->path, O_RDWR | O_NONBLOCK);
    if (inp->fd < 0) return 0;
    return 1;
}

static int open_ctl(void) {
    if (ctl_fd >= 0) return 1;
    ensure_fifo(ctl_path);
    ctl_fd = open(ctl_path, O_RDWR | O_NONBLOCK);
    return ctl_fd >= 0;
}

/* ── JPEG frame extraction ───────────────────────────────────── */

/*
 * Scan buffer for the last complete JPEG (SOI 0xFFD8 … EOI 0xFFD9).
 * Returns pointer into buf, sets *len.  NULL if none found.
 */
static uint8_t *last_jpeg(uint8_t *buf, size_t buflen, size_t *len) {
    uint8_t *best_soi = NULL;
    size_t   best_len = 0;

    size_t pos = 0;
    while (pos + 1 < buflen) {
        /* Find SOI */
        if (buf[pos] == 0xFF && buf[pos + 1] == 0xD8) {
            uint8_t *soi = buf + pos;
            /* Scan for matching EOI */
            size_t j = pos + 2;
            while (j + 1 < buflen) {
                if (buf[j] == 0xFF && buf[j + 1] == 0xD9) {
                    size_t flen = (j + 2) - pos;
                    if (flen <= MAX_FRAME_SIZE) {
                        best_soi = soi;
                        best_len = flen;
                    }
                    pos = j + 2;
                    goto next;
                }
                j++;
            }
            /* No EOI yet — partial frame, stop scanning */
            break;
        }
        pos++;
        next:;
    }

    if (best_soi) { *len = best_len; }
    return best_soi;
}

/* Drain an input FIFO and keep the latest complete frame */
static void drain(Input *inp) {
    for (;;) {
        size_t space = READ_BUF_SIZE - inp->read_used;
        if (space == 0) {
            /* Buffer full — discard first half */
            size_t half = READ_BUF_SIZE / 2;
            memmove(inp->read_buf, inp->read_buf + half, inp->read_used - half);
            inp->read_used -= half;
            space = READ_BUF_SIZE - inp->read_used;
        }
        ssize_t n = read(inp->fd, inp->read_buf + inp->read_used, space);
        if (n <= 0) break;
        inp->read_used += (size_t)n;
    }

    if (inp->read_used == 0) return;

    size_t flen = 0;
    uint8_t *frame = last_jpeg(inp->read_buf, inp->read_used, &flen);
    if (frame && flen > 0) {
        memcpy(inp->frame, frame, flen);
        inp->frame_size = flen;
        clock_gettime(CLOCK_MONOTONIC, &inp->frame_time);

        /* Discard everything up to and including this frame */
        size_t consumed = (size_t)(frame - inp->read_buf) + flen;
        if (consumed < inp->read_used) {
            memmove(inp->read_buf, inp->read_buf + consumed,
                    inp->read_used - consumed);
            inp->read_used -= consumed;
        } else {
            inp->read_used = 0;
        }
    }
}

/* ── Control pipe ────────────────────────────────────────────── */

static void process_ctl_line(char *line) {
    /* Trim whitespace */
    while (*line == ' ' || *line == '\t') line++;
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
                        || line[len-1] == ' ')) {
        line[--len] = '\0';
    }
    if (len == 0) return;

    if (strncmp(line, "source ", 7) == 0) {
        int n = atoi(line + 7);
        if (n >= 0 && n < num_inputs) {
            mode = MODE_SOURCE;
            selected = n;
            fprintf(stderr, "mux: switched to source %d (%s)\n",
                    n, inputs[n].path);
        }
    } else if (strcmp(line, "testcard") == 0) {
        mode = MODE_TESTCARD;
        selected = -1;
        fprintf(stderr, "mux: switched to testcard\n");
    } else if (strcmp(line, "auto") == 0) {
        mode = MODE_AUTO;
        selected = -1;
        fprintf(stderr, "mux: switched to auto\n");
    } else {
        fprintf(stderr, "mux: unknown command: %s\n", line);
    }
}

static void drain_ctl(void) {
    for (;;) {
        size_t space = CTL_BUF_SIZE - ctl_used - 1;
        if (space == 0) { ctl_used = 0; space = CTL_BUF_SIZE - 1; }
        ssize_t n = read(ctl_fd, ctl_buf + ctl_used, space);
        if (n <= 0) break;
        ctl_used += (size_t)n;
    }
    ctl_buf[ctl_used] = '\0';

    /* Process complete lines */
    char *start = ctl_buf;
    char *nl;
    while ((nl = strchr(start, '\n')) != NULL) {
        *nl = '\0';
        process_ctl_line(start);
        start = nl + 1;
    }

    /* Keep any partial line */
    size_t remaining = ctl_used - (size_t)(start - ctl_buf);
    if (remaining > 0 && start != ctl_buf) {
        memmove(ctl_buf, start, remaining);
    }
    ctl_used = remaining;
}

/* ── Frame selection ─────────────────────────────────────────── */

/* Returns pointer to frame data + sets *len.  NULL = nothing. */
static uint8_t *choose_frame(size_t *len) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (mode == MODE_TESTCARD) goto use_testcard;

    if (mode == MODE_SOURCE && selected >= 0 && selected < num_inputs) {
        Input *inp = &inputs[selected];
        if (inp->frame_size > 0 &&
            elapsed_ms(&inp->frame_time, &now) < timeout_ms) {
            *len = inp->frame_size;
            return inp->frame;
        }
        /* Selected source stale — fall through to testcard */
        goto use_testcard;
    }

    /* MODE_AUTO: pick highest-index input with a recent frame */
    for (int i = num_inputs - 1; i >= 0; i--) {
        Input *inp = &inputs[i];
        if (inp->frame_size > 0 &&
            elapsed_ms(&inp->frame_time, &now) < timeout_ms) {
            *len = inp->frame_size;
            return inp->frame;
        }
    }

use_testcard:
    if (testcard && testcard_size > 0) {
        *len = testcard_size;
        return testcard;
    }
    return NULL;
}

/* ── Output (stdout) ─────────────────────────────────────────── */

static int write_stdout(const uint8_t *data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t w = write(STDOUT_FILENO, data + total, len - total);
        if (w < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd = { .fd = STDOUT_FILENO, .events = POLLOUT };
                if (poll(&pfd, 1, 200) <= 0) return 0;
                continue;
            }
            return 0;
        }
        if (w == 0) return 0;
        total += (size_t)w;
    }

    /* FPS tracking */
    out_frames++;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double secs = elapsed_ms(&fps_start, &now) / 1000.0;
    if (secs >= 10.0) {
        fprintf(stderr, "mux: %.1f fps (%d frames in %.0fs)\n",
                out_frames / secs, out_frames, secs);
        out_frames = 0;
        fps_start = now;
    }
    return 1;
}

/* ── Main ────────────────────────────────────────────────────── */

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -n N            Number of input FIFOs (default: 2)\n"
        "  -t path.jpg     Testcard JPEG file\n"
        "  -f fps           Target output FPS (default: 10)\n"
        "  -T ms            Source timeout in ms (default: 500)\n"
        "  -c path          Control pipe path (default: /tmp/folk-mux-ctl.fifo)\n"
        "  -p prefix        Input FIFO prefix (default: /tmp/folk-mux-)\n"
        "\n"
        "Input FIFOs: <prefix>0.fifo, <prefix>1.fifo, ...\n"
        "Control:     echo 'source 0' > /tmp/folk-mux-ctl.fifo\n",
        prog);
    exit(1);
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    char prefix[200] = "/tmp/folk-mux-";
    char testcard_path[256] = "";
    int n_inputs = 2;

    int opt;
    while ((opt = getopt(argc, argv, "n:t:f:T:c:p:h")) != -1) {
        switch (opt) {
        case 'n': n_inputs = atoi(optarg); break;
        case 't': strncpy(testcard_path, optarg, sizeof(testcard_path)-1); break;
        case 'f': target_fps = atoi(optarg); break;
        case 'T': timeout_ms = atoi(optarg); break;
        case 'c': strncpy(ctl_path, optarg, sizeof(ctl_path)-1); break;
        case 'p': strncpy(prefix, optarg, sizeof(prefix)-1); break;
        default:  usage(argv[0]);
        }
    }

    if (n_inputs < 1) n_inputs = 1;
    if (n_inputs > MAX_INPUTS) n_inputs = MAX_INPUTS;
    num_inputs = n_inputs;

    /* Load testcard */
    if (testcard_path[0]) {
        load_testcard(testcard_path);
    }

    /* Initialize inputs */
    for (int i = 0; i < num_inputs; i++) {
        Input *inp = &inputs[i];
        snprintf(inp->path, sizeof(inp->path), "%s%d.fifo", prefix, i);
        inp->fd = -1;
        inp->read_buf = calloc(1, READ_BUF_SIZE);
        inp->read_used = 0;
        inp->frame = calloc(1, MAX_FRAME_SIZE);
        inp->frame_size = 0;
    }

    fprintf(stderr, "folk-stream-mux: %d inputs, fps=%d, timeout=%dms\n",
            num_inputs, target_fps, timeout_ms);
    for (int i = 0; i < num_inputs; i++)
        fprintf(stderr, "  input[%d]: %s\n", i, inputs[i].path);
    if (testcard) fprintf(stderr, "  testcard: %s\n", testcard_path);
    fprintf(stderr, "  control:  %s\n", ctl_path);

    int interval_ms = 1000 / target_fps;
    struct timespec last_out = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &fps_start);

    while (running) {
        /* (Re)open pipes */
        for (int i = 0; i < num_inputs; i++) open_input(&inputs[i]);
        open_ctl();

        /* Build poll set: inputs + control */
        struct pollfd fds[MAX_INPUTS + 1];
        int map[MAX_INPUTS + 1];   /* map[poll_idx] → input idx, or -1 for ctl */
        int nfds = 0;

        for (int i = 0; i < num_inputs; i++) {
            if (inputs[i].fd >= 0) {
                fds[nfds].fd = inputs[i].fd;
                fds[nfds].events = POLLIN;
                map[nfds] = i;
                nfds++;
            }
        }
        if (ctl_fd >= 0) {
            fds[nfds].fd = ctl_fd;
            fds[nfds].events = POLLIN;
            map[nfds] = -1;
            nfds++;
        }

        if (nfds == 0) { usleep(100000); continue; }

        poll(fds, nfds, interval_ms);

        /* Process ready fds */
        for (int fi = 0; fi < nfds; fi++) {
            if (!(fds[fi].revents & POLLIN)) continue;
            if (map[fi] == -1) {
                drain_ctl();
            } else {
                drain(&inputs[map[fi]]);
            }
        }

        /* Output at target FPS */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (last_out.tv_sec == 0 || elapsed_ms(&last_out, &now) >= interval_ms) {
            last_out = now;

            size_t flen = 0;
            uint8_t *frame = choose_frame(&flen);
            if (frame && flen > 0) {
                if (!write_stdout(frame, flen)) {
                    fprintf(stderr, "mux: stdout write failed, exiting\n");
                    running = 0;
                }
            }
        }
    }

    /* Cleanup */
    for (int i = 0; i < num_inputs; i++) {
        if (inputs[i].fd >= 0) close(inputs[i].fd);
        free(inputs[i].read_buf);
        free(inputs[i].frame);
    }
    if (ctl_fd >= 0) close(ctl_fd);
    free(testcard);

    fprintf(stderr, "folk-stream-mux: done\n");
    return 0;
}
