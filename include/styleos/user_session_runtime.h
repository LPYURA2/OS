#ifndef STYLEOS_USER_SESSION_RUNTIME_H
#define STYLEOS_USER_SESSION_RUNTIME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <styleos/process_runtime.h>
#include <styleos/window_service.h>

/* Kernel-only preparation for per-user desktops; not an authentication API.
 * Do NOT accept account ids or inspect grants from untrusted app code. */
enum { USR_MAX_SESSIONS = 4 };
enum usr_status {
    USR_OK = 0, USR_INVALID, USR_DENIED, USR_FULL,
    USR_MISSING, USR_BUSY, USR_STALE, USR_EXHAUSTED
};
enum usr_state { USR_CLOSED = 0, USR_LOCKED = 1, USR_ACTIVE = 2 };
struct usr_session {
    uint32_t id;
    uint64_t account_id; /* opaque authenticated identity, NOT a password */
    enum usr_state state;
};
struct usr_owner {
    uint32_t pid, session_id;
    uint64_t scheduler_handle; /* trusted kernel evidence, never sent to UI */
};
struct usr_process_row {
    uint32_t pid, parent_pid;
    enum pr_state state;
    char label[PR_LABEL_BYTES]; /* no handles, CR3 roots, or fake metrics */
};
struct usr_runtime {
    struct pr_table *processes;
    struct ws_service *windows;
    uint32_t next_session;
    uint32_t selected_session;
    bool ready;
    struct usr_session sessions[USR_MAX_SESSIONS];
    struct usr_owner owners[PR_MAX_PROCESSES];
};
/* Refuse dirty runtime state; never transform shell session ids into PID. */
enum usr_status usr_init(struct usr_runtime *, struct pr_table *, struct ws_service *);
/* All session selection/creation functions are called by trusted kernel/auth
 * broker only; they do NOT authenticate the account or enforce file ACLs. */
enum usr_status usr_create(struct usr_runtime *, uint64_t authenticated_account_id,
                           uint32_t *session_id);
enum usr_status usr_select(struct usr_runtime *, uint32_t session_id);
enum usr_status usr_lock(struct usr_runtime *);
enum usr_status usr_close(struct usr_runtime *, uint32_t session_id);
/* Register ONLY a live scheduler-admitted native user PID and its REAL handle.
 * The parent, when present, must belong to the same user session. */
enum usr_status usr_bind(struct usr_runtime *, uint32_t session_id,
                         uint32_t pid, uint64_t authenticated_handle);
/* Preserve live apps across shell/compositor restart; drop dead bindings only
 * AFTER their windows are reaped. No cross-session window fallback. */
size_t usr_reconcile(struct usr_runtime *);
size_t usr_visible_windows(const struct usr_runtime *, struct ws_window *output,
                           size_t capacity);
/* Read-only process snapshot: a live caller sees own session only. Higher
 * privilege is intentionally NOT accepted as a user-supplied argument. */
enum usr_status usr_processes(const struct usr_runtime *, uint32_t caller_pid,
                              struct usr_process_row *output, size_t capacity,
                              size_t *total);
const struct usr_session *usr_find(const struct usr_runtime *,uint32_t session_id);
/* Empty boot singleton; never creates a fake profile or native process. */
enum usr_status usr_global_start(void);
const struct usr_runtime *usr_global(void);
#endif
