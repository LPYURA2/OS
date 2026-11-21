#ifndef STYLEOS_PROCESS_VIEW_H
#define STYLEOS_PROCESS_VIEW_H
#include <styleos/process_inspector.h>
enum pi_order { PI_BY_PID=1, PI_BY_NAME, PI_BY_CPU_TICKS, PI_BY_MEMORY, PI_BY_STATE };
enum { PI_MAX_SEARCH = 64 };
struct pi_query {
    enum pi_order by;
    bool descending;
    const char *search;
};
struct pi_view_info {
    struct pi_snapshot_info source;
    size_t matched;
};
/* Read-only projection. The real scheduler must synchronize snapshots.
 * Never includes legacy desktop session IDs. */
enum pi_status pi_view(const struct pr_table *processes,
                        const struct ws_service *windows,
                        const struct pi_query *query,
                        struct pi_row *rows, size_t capacity,
                        struct pi_view_info *info);
#endif
