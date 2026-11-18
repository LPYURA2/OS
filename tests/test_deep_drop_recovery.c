#define _POSIX_C_SOURCE 200809L

#include <styleos/deep_drop.h>
#include <styleos/sdk_v0.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* A host-only pipe protocol. The execution barrier is for fault injection,
 * not a guest IPC or security boundary. */
enum { READY = 1, RECEIVED, CONTINUE };

struct worker {
    pid_t pid;
    int request;
    int reply;
};

struct supervisor {
    struct worker worker;
    unsigned restarts;
    int last_status;
    bool inject_crash_after_receipt;
};

struct slide {
    uint8_t text[STYLEOS_TEXT_MAX];
    size_t size;
    unsigned applies;
};

static bool receive(int fd, void *buffer, size_t size)
{
    uint8_t *next = buffer;
    while (size) {
        ssize_t n = read(fd, next, size);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return false;
        }
        next += n;
        size -= (size_t)n;
    }
    return true;
}

static bool send_bytes(int fd, const void *buffer, size_t size)
{
    const uint8_t *next = buffer;
    while (size) {
        ssize_t n = write(fd, next, size);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return false;
        }
        next += n;
        size -= (size_t)n;
    }
    return true;
}

static bool expect_marker(int fd, uint8_t expected)
{
    uint8_t actual;
    return receive(fd, &actual, sizeof(actual)) && actual == expected;
}

static int worker_loop(int request, int reply)
{
    uint8_t input[STYLEOS_TEXT_MAX];
    uint8_t output[STYLEOS_TEXT_MAX];
    const uint8_t ready = READY, received = RECEIVED;
    if (!send_bytes(reply, &ready, sizeof(ready))) {
        return EXIT_FAILURE;
    }

    uint32_t size;
    while (receive(request, &size, sizeof(size))) {
        if (size > sizeof(input) || !receive(request, input, size) ||
            !send_bytes(reply, &received, sizeof(received)) ||
            !expect_marker(request, CONTINUE)) {
            return EXIT_FAILURE;
        }
        uint32_t written = styleos_app_text_v0(input, size, output, sizeof(output));
        if ((written != STYLEOS_TEXT_REJECTED && written > sizeof(output)) ||
            !send_bytes(reply, &written, sizeof(written)) ||
            (written != STYLEOS_TEXT_REJECTED && !send_bytes(reply, output, written))) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

static struct worker start_worker(void)
{
    int request[2], reply[2];
    assert(pipe(request) == 0);
    assert(pipe(reply) == 0);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        assert(close(request[1]) == 0);
        assert(close(reply[0]) == 0);
        _exit(worker_loop(request[0], reply[1]));
    }
    assert(close(request[0]) == 0);
    assert(close(reply[1]) == 0);
    assert(expect_marker(reply[0], READY));
    return (struct worker){pid, request[1], reply[0]};
}

static int stop_and_reap(struct worker worker)
{
    assert(close(worker.request) == 0);
    assert(close(worker.reply) == 0);
    int status;
    pid_t waited;
    do {
        waited = waitpid(worker.pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    assert(waited == worker.pid);
    return status;
}

/* The real transfer core sees only a failed prepare. The adapter observes
 * pipe failure on the old worker and restarts it without replaying the offer. */
static bool prepare(void *context, const struct styleos_drop_offer *offer,
                    uint8_t *staging, size_t capacity, size_t *written)
{
    struct supervisor *supervisor = context;
    const struct worker worker = supervisor->worker;
    assert(offer->size <= STYLEOS_TEXT_MAX);
    uint32_t size = (uint32_t)offer->size;
    if (!send_bytes(worker.request, &size, sizeof(size)) ||
        !send_bytes(worker.request, offer->data, size) ||
        !expect_marker(worker.reply, RECEIVED)) {
        goto worker_lost;
    }

    if (supervisor->inject_crash_after_receipt) {
        supervisor->inject_crash_after_receipt = false;
        /* The child has the complete offer, but cannot invoke the SDK or
         * produce a reply until this side sends CONTINUE. */
        assert(kill(worker.pid, SIGKILL) == 0);
        printf("PASS worker %ld received full offer, then was killed before reply\n", (long)worker.pid);
    } else {
        const uint8_t proceed = CONTINUE;
        if (!send_bytes(worker.request, &proceed, sizeof(proceed))) {
            goto worker_lost;
        }
    }

    uint32_t length;
    if (!receive(worker.reply, &length, sizeof(length))) {
        goto worker_lost;
    }
    if (length == STYLEOS_TEXT_REJECTED) {
        return false;
    }
    if (length > capacity || !receive(worker.reply, staging, length)) {
        goto worker_lost;
    }
    *written = length;
    return true;

worker_lost:
    supervisor->last_status = stop_and_reap(worker);
    supervisor->worker = start_worker();
    ++supervisor->restarts;
    return false;
}

static void apply(void *context, const uint8_t *data, size_t size)
{
    struct slide *slide = context;
    assert(size <= sizeof(slide->text));
    memcpy(slide->text, data, size);
    slide->size = size;
    ++slide->applies;
}

static size_t read_selected(const char *path, uint8_t *buffer, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t size = fread(buffer, 1, capacity, file);
    assert(!ferror(file) && feof(file));
    assert(fclose(file) == 0);
    return size;
}

static void assert_source(const char *path, const uint8_t *expected, size_t size)
{
    uint8_t current[STYLEOS_TEXT_MAX + 1];
    assert(read_selected(path, current, sizeof(current)) == size);
    assert(memcmp(current, expected, size) == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
    const pid_t owner_pid = getpid();
    struct supervisor supervisor = {.worker = start_worker()};
    const pid_t failed_pid = supervisor.worker.pid;
    struct slide slide = {0};
    const char *const types[] = {"text/plain"};
    const struct styleos_drop_target target = {
        .accepted_types = types, .accepted_type_count = 1,
        .max_input_size = STYLEOS_TEXT_MAX, .prepare = prepare,
        .prepare_context = &supervisor, .apply = apply, .apply_context = &slide,
    };
    const uint8_t old[] = "Предыдущий слайд\n";
    const struct styleos_drop_offer original = {"text/plain", old, sizeof(old) - 1};
    uint8_t staging[STYLEOS_TEXT_MAX], selected[STYLEOS_TEXT_MAX + 1];
    assert(styleos_drop_transfer(&original, &target, staging, sizeof(staging)) == STYLEOS_DROP_OK);
    assert(slide.applies == 1);
    puts("PASS initial transfer through the persistent worker");

    size_t size = read_selected(argv[1], selected, sizeof(selected));
    assert(size > 0 && size <= STYLEOS_TEXT_MAX);
    const struct styleos_drop_offer incoming = {"text/plain", selected, size};
    supervisor.inject_crash_after_receipt = true;
    assert(styleos_drop_transfer(&incoming, &target, staging, sizeof(staging)) == STYLEOS_DROP_PREPARE_FAILED);
    assert(WIFSIGNALED(supervisor.last_status) && WTERMSIG(supervisor.last_status) == SIGKILL);
    assert(supervisor.restarts == 1 && supervisor.worker.pid != failed_pid);
    printf("PASS observed SIGKILL and automatically started ready worker %ld\n", (long)supervisor.worker.pid);
    assert(getpid() == owner_pid && slide.applies == 1 && slide.size == original.size);
    assert(memcmp(slide.text, old, original.size) == 0);
    assert_source(argv[1], selected, size);
    puts("PASS host caller stays alive, old slide and source unchanged, no automatic replay");

    assert(styleos_drop_transfer(&incoming, &target, staging, sizeof(staging)) == STYLEOS_DROP_OK);
    assert(supervisor.restarts == 1 && getpid() == owner_pid);
    assert(slide.applies == 2 && slide.size == size && memcmp(slide.text, selected, size) == 0);
    assert_source(argv[1], selected, size);
    puts("PASS explicit retry applies the complete transfer exactly once");
    int status = stop_and_reap(supervisor.worker);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS);
    puts("PASS replacement worker shuts down and is reaped normally");
    return 0;
}
