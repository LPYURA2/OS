#include <styleos/desktop_process_runtime.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct pr_table processes;
static struct ws_service windows;
static struct dpr_service service;
static struct pa_row rows[PA_MAX_ROWS];
static struct pa_reply response;

int main(void)
{
    struct pa_request q = { .version = PA_VERSION, .order = PI_BY_PID };
    uint32_t a = 0, b = 0, win = 0, seen = 0;
    unsigned slot = 99;
    pr_init(&processes); ws_init(&windows);
    assert(dpr_global_state() == 0);
    assert(dpr_dispatch(&service, &slot, &seen) == PS_STATE);
    assert(dpr_start(&service, &processes, &windows, 0) == DPR_ARGUMENT);
    assert(dpr_start(&service, &processes, &windows, 0x1001) == DPR_ARGUMENT);
    assert(dpr_start(&service, &processes, &windows, 0x1000) == DPR_OK);
    assert(dpr_start(&service, &processes, &windows, 0x1000) == DPR_ALREADY_STARTED);
    assert(pr_snapshot(&processes, 0, 0, false) == 0);
    assert(ws_snapshot(&windows, 0, 0) == 0);
    memset(rows, 0x5a, sizeof(rows)); memset(&response, 0x5a, sizeof(response));
    assert(dpr_snapshot(&service, 0, false, &q, rows, PA_MAX_ROWS, &response) == PA_DENIED);
    assert(rows[0].pid == 0x5a5a5a5au && response.version == 0x5a5a5a5au);
    assert(dpr_snapshot(&service, 0, true, &q, rows, PA_MAX_ROWS, &response) == PA_DENIED);
    assert(dpr_admit(&service, 0, 101, 0x2000, 0, "Editor.tea", &a) == PS_OK);
    assert(dpr_admit(&service, 1, 102, 0x3000, 0, "Processes.tea", &b) == PS_OK);
    assert(a && b > a);
    assert(dpr_admit(&service, 1, 103, 0x4000, 0, "Duplicate", &seen) == PS_STATE);
    assert(ws_open_app(&windows, &processes, a, "Editor", (struct ws_bounds){0,0,400,300}, &win) == WS_OK);
    assert(win != 0);
    assert(dpr_snapshot(&service, b, false, &q, rows, PA_MAX_ROWS, &response) == PA_DENIED);
    assert(dpr_snapshot(&service, b, true, &q, rows, PA_MAX_ROWS, &response) == PA_OK);
    assert(response.total == 2 && response.applications == 1 && response.background == 1);
    assert(response.system == 0 && response.measurement_flags == 0);
    assert(rows[0].pid == a && rows[0].group == PI_APPLICATIONS && rows[0].open_windows == 1);
    assert(rows[1].pid == b && rows[1].group == PI_BACKGROUND && rows[1].open_windows == 0);
    assert(rows[0].cpu_ticks == 0 && rows[0].resident_bytes == 0);
    assert(dpr_dispatch(&service, &slot, &seen) == PS_OK && slot == 0 && seen == a);
    assert(pr_find(&processes, a)->state == PR_RUNNING);
    assert(dpr_observe(&service, slot, 101, 0x2000, 0x9999, PS_YIELD) == PS_EVIDENCE);
    assert(pr_find(&processes, a)->state == PR_RUNNING);
    assert(dpr_observe(&service, slot, 101, 0x2000, 0x1000, PS_YIELD) == PS_OK);
    assert(pr_find(&processes, a)->state == PR_READY);
    assert(ws_compositor_restart(&windows) == 2);
    assert(ws_find(&windows, win) != 0);
    assert(dpr_dispatch(&service, &slot, &seen) == PS_OK && slot == 1 && seen == b);
    assert(dpr_observe(&service, slot, 102, 0x3000, 0x1000, PS_FAULT) == PS_OK);
    assert(!pr_is_live(&processes, b));
    assert(dpr_snapshot(&service, b, true, &q, rows, PA_MAX_ROWS, &response) == PA_DENIED);
    assert(dpr_dispatch(&service, &slot, &seen) == PS_OK && slot == 0 && seen == a);
    assert(dpr_observe(&service, slot, 101, 0x2000, 0x1000, PS_EXIT) == PS_OK);
    assert(ws_find(&windows, win) == 0);
    assert(!pr_is_live(&processes, a));
    assert(pr_snapshot(&processes, 0, 0, false) == 0);
    assert(dpr_release(&service, 1, 999) == PS_EVIDENCE);
    assert(dpr_release(&service, 1, 102) == PS_OK);
    assert(dpr_release(&service, 0, 101) == PS_OK);
    assert(pr_snapshot(&processes, 0, 0, true) == 0);
    assert(dpr_admit(&service, 0, 201, 0x4000, 0, "New.tea", &seen) == PS_OK);
    assert(seen > b);
    puts("DESKTOP PROCESS RUNTIME HOST PASS: no fake PID, boot gate, snapshots, F10, trap cleanup");
    return 0;
}
