#include <styleos/process_inspector.h>
static int live(enum pr_state s)
{ return s == PR_READY || s == PR_RUNNING || s == PR_WAITING; }
static unsigned window_count(const struct ws_service *windows, uint32_t pid)
{
    unsigned n = 0;
    for (unsigned i = 0; i < WS_MAX_WINDOWS; ++i) {
        const struct ws_window *w = &windows->windows[i];
        if (w->id && w->role == WS_APP_WINDOW && w->owner_pid == pid)
            ++n;
    }
    return n;
}
static enum pi_group group_for(const struct pr_entry *p, unsigned windows)
{
    if (p->type == PR_KERNEL_TASK) return PI_SYSTEM;
    return windows ? PI_APPLICATIONS : PI_BACKGROUND;
}
enum pi_status pi_collect(const struct pr_table *processes,
                          const struct ws_service *windows,
                          struct pi_row *rows, size_t capacity,
                          struct pi_snapshot_info *info)
{
    if (!processes || !windows || !info || (capacity && !rows))
        return PI_ARGUMENT;
    struct pi_snapshot_info result = {0};
    result.process_serial = processes->change_serial;
    result.window_generation = windows->compositor_generation;
    /* CPU % stays unavailable until a scheduler clock and interval-based
     * accounting exist. CPU ticks are raw kernel evidence, not a percentage. */
    result.cpu_percentage_available = false;
    size_t written = 0;
    for (unsigned group = PI_APPLICATIONS; group <= PI_SYSTEM; ++group) {
        /* Reaping reuses slots, not PIDs. Sort by real PID even for truncated
         * results. The fixed registry bounds the selection scan. */
        uint32_t after = 0;
        for (unsigned step = 0; step < PR_MAX_PROCESSES; ++step) {
            const struct pr_entry *p = 0;
            unsigned windows_for_pid = 0;
            for (unsigned i = 0; i < PR_MAX_PROCESSES; ++i) {
                const struct pr_entry *candidate = &processes->processes[i];
                if (candidate->pid <= after || !live(candidate->state) ||
                    (p && candidate->pid >= p->pid)) continue;
                const unsigned n = window_count(windows, candidate->pid);
                if ((unsigned)group_for(candidate,n) != group) continue;
                p = candidate; windows_for_pid = n;
            }
            if (!p) break;
            after = p->pid;
            const enum pi_group category = (enum pi_group)group;
            ++result.total;
            if (category == PI_APPLICATIONS) ++result.applications;
            else if (category == PI_BACKGROUND) ++result.background;
            else ++result.system;
            if (written < capacity) {
                struct pi_row *r = &rows[written++];
                r->pid = p->pid;
                r->parent_pid = p->parent_pid;
                r->group = category;
                r->type = p->type;
                r->state = p->state;
                r->cpu_ticks = p->cpu_ticks;
                r->resident_bytes = p->resident_bytes;
                r->open_windows = windows_for_pid;
                for (unsigned j = 0; j < PR_LABEL_BYTES; ++j)
                    r->name[j] = p->label[j];
            }
        }
    }
    *info = result;
    return result.total <= capacity ? PI_OK : PI_TRUNCATED;
}
enum pi_status pi_can_request_end(const struct pr_table *processes,
                                  uint32_t target_pid)
{
    const struct pr_entry *target = pr_find(processes, target_pid);
    if (!target || !live(target->state)) return PI_TARGET_MISSING;
    if (target->type != PR_NATIVE_USER) return PI_TARGET_PROTECTED;
    return PI_TARGET_ELIGIBLE;
}
