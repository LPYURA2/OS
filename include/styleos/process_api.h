#ifndef STYLEOS_PROCESS_API_H
#define STYLEOS_PROCESS_API_H

#include <styleos/process_view.h>

/* Kernel-only staging contract for a future read-only process snapshot syscall.
 * This is NOT a userspace syscall and none of these pointers may be accepted
 * directly from CPL3. The syscall broker must first copy/validate the request
 * and authenticate the caller's PID and inspect capability in kernel space. */
enum { PA_VERSION = 1, PA_NAME_BYTES = PR_LABEL_BYTES,
       PA_SEARCH_BYTES = PI_MAX_SEARCH, PA_MAX_ROWS = PR_MAX_PROCESSES };
enum pa_status { PA_OK = 0, PA_BAD_REQUEST, PA_DENIED, PA_TRUNCATED };

/* Fixed-width data: no pointers, scheduler handles or page-table roots. */
struct pa_request {
    uint32_t version;
    uint32_t order;                 /* PI_BY_* */
    uint32_t descending;            /* 0 or 1 */
    uint32_t reserved;              /* must be zero */
    char search[PA_SEARCH_BYTES];   /* NUL-terminated, ASCII case fold */
};
struct pa_row {
    uint32_t pid, parent_pid, group, type, state, open_windows;
    uint64_t cpu_ticks, resident_bytes;
    char name[PA_NAME_BYTES];
};
struct pa_reply {
    uint32_t version;
    uint32_t process_serial;
    uint32_t window_generation;
    uint32_t total, matched, returned;
    uint32_t applications, background, system;
    /* Validity mask is deliberately zero until scheduler and memory accounting
     * feed observed measurements. Raw ticks/bytes are NOT percentages. */
    uint32_t measurement_flags;
};
enum { PA_MEASURE_CPU_TICKS = 1u, PA_MEASURE_MEMORY_BYTES = 2u,
       PA_MEASURE_CPU_PERCENT = 4u };

/* kernel_authenticated_pid + kernel_inspect_granted are values supplied by a
 * trusted kernel broker, NEVER by user input. Caller must still be live.
 * Scheduler/window state must be locked by the eventual broker. */
enum pa_status pa_read(const struct pr_table *processes,
                       const struct ws_service *windows,
                       uint32_t kernel_authenticated_pid,
                       bool kernel_inspect_granted,
                       const struct pa_request *request,
                       struct pa_row *rows, size_t capacity,
                       struct pa_reply *reply);
#endif
