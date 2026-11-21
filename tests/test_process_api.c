#include <styleos/process_api.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct pr_table p;
static struct ws_service w;
static struct pa_row rows[PA_MAX_ROWS];
static struct pa_reply reply;

static uint32_t add(enum pr_type kind, uint32_t parent, uint64_t handle,
                    uint64_t root, uint64_t memory, const char *name)
{
    const struct pr_admission a = {kind, parent, handle, root, memory, name};
    uint32_t pid = 0;
    assert(pr_admit(&p, &a, &pid) == PR_OK);
    return pid;
}
int main(void)
{
    pr_init(&p); ws_init(&w);
    const uint32_t kernel = add(PR_KERNEL_TASK, 0, 101, 0, 4096, "kernel");
    const uint32_t inspector = add(PR_NATIVE_USER, kernel, 102, 0x2000,
                                    8192, "Processes");
    const uint32_t editor = add(PR_NATIVE_USER, kernel, 103, 0x3000,
                                 12000, "Editor");
    const uint32_t helper = add(PR_NATIVE_USER, kernel, 104, 0x4000,
                                 24000, "helper");
    struct ws_bounds b = {1, 2, 200, 100}; uint32_t win = 0;
    assert(ws_open_app(&w, &p, editor, "Editor", b, &win) == WS_OK);
    struct pa_request q = {.version=PA_VERSION, .order=PI_BY_PID};

    memset(rows, 0x5a, sizeof(rows)); memset(&reply, 0x5a, sizeof(reply));
    assert(pa_read(&p, &w, inspector, false, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_DENIED);
    assert(rows[0].pid == 0x5a5a5a5au && reply.version == 0x5a5a5a5au);
    assert(pa_read(&p, &w, 999, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_DENIED);
    assert(rows[0].pid == 0x5a5a5a5au);
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_OK);
    assert(reply.version == PA_VERSION && reply.total == 4 &&
           reply.matched == 4 && reply.returned == 4);
    assert(reply.applications == 1 && reply.background == 2 && reply.system == 1);
    assert(reply.measurement_flags == 0);
    assert(rows[0].pid == editor && rows[0].open_windows == 1);
    assert(rows[1].pid == inspector && rows[2].pid == helper);
    assert(rows[3].pid == kernel);
    assert(rows[0].parent_pid == kernel && rows[0].type == PR_NATIVE_USER);

    q.order = PI_BY_MEMORY; q.descending = 1;
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_OK);
    assert(rows[1].pid == helper && rows[2].pid == inspector);
    q.order = PI_BY_PID; q.descending = 0;
    strcpy(q.search, "EDItor");
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_OK);
    assert(reply.matched == 1 && rows[0].pid == editor);
    strcpy(q.search, "");
    assert(pa_read(&p, &w, inspector, true, &q, rows, 2,
                   &reply) == PA_TRUNCATED);
    assert(reply.matched == 4 && reply.returned == 2);
    assert(pa_read(&p, &w, inspector, true, &q, 0, 0,
                   &reply) == PA_TRUNCATED && reply.returned == 0);

    q.version = PA_VERSION + 1;
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_BAD_REQUEST);
    q.version = PA_VERSION; q.reserved = 9;
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_BAD_REQUEST);
    q.reserved = 0; q.descending = 2;
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_BAD_REQUEST);
    q.descending = 0; q.order = PI_BY_PID;
    memset(q.search, 'x', sizeof(q.search));
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_BAD_REQUEST);
    q.search[0] = 0;
    assert(pa_read(&p, &w, inspector, true, &q, 0, 1,
                   &reply) == PA_BAD_REQUEST);
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS+1u,
                   &reply) == PA_BAD_REQUEST);
    assert(ws_compositor_restart(&w) == 2);
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_OK);
    assert(reply.window_generation == 2 && rows[0].pid == editor);
    assert(pr_transition(&p, inspector, 102, PR_FAULTED) == PR_OK);
    assert(pa_read(&p, &w, inspector, true, &q, rows, PA_MAX_ROWS,
                   &reply) == PA_DENIED);
    puts("PROCESS SNAPSHOT API PASS: read-only, rights, bounds, groups, F10, no invented metrics");
    return 0;
}
