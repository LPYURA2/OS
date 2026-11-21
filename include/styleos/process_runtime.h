#ifndef STYLEOS_PROCESS_RUNTIME_H
#define STYLEOS_PROCESS_RUNTIME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Registry contains scheduler-admitted contexts, NEVER UI session records. */
enum { PR_MAX_PROCESSES = 32, PR_LABEL_BYTES = 64 };
enum pr_type { PR_KERNEL_TASK = 1, PR_NATIVE_USER = 2 };
enum pr_state { PR_UNUSED = 0, PR_READY = 1, PR_RUNNING = 2,
                PR_WAITING = 3, PR_EXITED = 4, PR_FAULTED = 5 };
enum pr_status { PR_OK = 0, PR_BAD_ARGUMENT, PR_BAD_EVIDENCE,
                 PR_CONFLICT, PR_FULL, PR_NOT_FOUND, PR_INVALID_STATE,
                 PR_EXHAUSTED };
struct pr_admission {
    enum pr_type type;
    uint32_t parent_pid;
    uint64_t scheduler_handle; /* opaque nonzero handle issued by scheduler */
    uint64_t page_root;        /* nonzero page-aligned for user processes */
    uint64_t resident_bytes;
    const char *label;         /* canonical app or named kernel task */
};
struct pr_entry {
    uint32_t pid, parent_pid;
    enum pr_type type;
    enum pr_state state;
    uint64_t scheduler_handle, page_root, resident_bytes, cpu_ticks;
    char label[PR_LABEL_BYTES];
};
struct pr_table {
    uint32_t next_pid;
    uint32_t change_serial;
    struct pr_entry processes[PR_MAX_PROCESSES];
};
void pr_init(struct pr_table *table);
/* Runtime-owned registry; never populated from legacy F5 session IDs. */
struct pr_table *pr_global(void);
enum pr_status pr_admit(struct pr_table *table, const struct pr_admission *spec,
                        uint32_t *pid);
enum pr_status pr_transition(struct pr_table *table, uint32_t pid,
                             uint64_t scheduler_handle, enum pr_state next);
enum pr_status pr_account(struct pr_table *table, uint32_t pid,
                          uint64_t scheduler_handle, uint64_t delta_ticks,
                          uint64_t resident_bytes);
enum pr_status pr_reap(struct pr_table *table, uint32_t pid,
                       uint64_t scheduler_handle);
const struct pr_entry *pr_find(const struct pr_table *table, uint32_t pid);
bool pr_is_live(const struct pr_table *table, uint32_t pid);
size_t pr_snapshot(const struct pr_table *table, struct pr_entry *output,
                   size_t capacity, bool include_terminated);
#endif
