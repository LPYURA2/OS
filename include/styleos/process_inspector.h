#ifndef STYLEOS_PROCESS_INSPECTOR_H
#define STYLEOS_PROCESS_INSPECTOR_H
#include <styleos/process_runtime.h>
#include <styleos/window_service.h>
/* A read-only Task-Manager-style view over real kernel process records.
 * Windows are used ONLY for grouping, never as a source of fabricated PIDs.
 * All calls are kernel-internal until a capability-checked read-only IPC API
 * is implemented. Caller must hold the scheduler snapshot lock in future. */
enum pi_group {
    PI_APPLICATIONS = 1,
    PI_BACKGROUND = 2,
    PI_SYSTEM = 3
};
enum pi_status {
    PI_OK = 0, PI_ARGUMENT, PI_TRUNCATED,
    PI_TARGET_MISSING, PI_TARGET_PROTECTED, PI_TARGET_ELIGIBLE
};
struct pi_row {
    uint32_t pid, parent_pid;
    enum pi_group group;
    enum pr_type type;
    enum pr_state state;
    uint64_t cpu_ticks, resident_bytes;
    unsigned open_windows;
    char name[PR_LABEL_BYTES];
};
struct pi_snapshot_info {
    uint32_t process_serial;
    uint32_t window_generation;
    size_t total, applications, background, system;
    bool cpu_percentage_available;
};
/* Groups in Task Manager order: Apps, Background processes, System.
 * Rows ordered by monotonically issued PID within each group. */
enum pi_status pi_collect(const struct pr_table *processes,
                          const struct ws_service *windows,
                          struct pi_row *rows, size_t capacity,
                          struct pi_snapshot_info *info);
/* Only preflight policy; NEVER terminates a process.
 * Kernel must separately authorize caller, owning user, and scheduler. */
enum pi_status pi_can_request_end(const struct pr_table *processes,
                                  uint32_t target_pid);
#endif
