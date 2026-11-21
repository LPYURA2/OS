#ifndef STYLEOS_NATIVE_LAUNCH_STAGE_H
#define STYLEOS_NATIVE_LAUNCH_STAGE_H

#include <styleos/desktop_frame_owner.h>
#include <styleos/desktop_process_runtime.h>
#include <styleos/user_session_runtime.h>

/* Kernel-private, single-CPU transaction staging for native app processes.
 * No user-mode ABI, ELF interpreter, CPU switch, .tea launcher or syscall.
 * The caller MUST be an independently trusted arch/package loader which has
 * already authenticated the session and attested all memory/entry mappings. */
enum nla_status {
    NLA_OK = 0, NLA_ARGUMENT, NLA_NOT_READY, NLA_CONFLICT,
    NLA_CONTEXT, NLA_ADMISSION, NLA_FRAMES, NLA_SESSION,
    NLA_WINDOW, NLA_NOT_TERMINATED, NLA_STALE, NLA_ROLLBACK_FAILED
};
struct nla_record {
    uint32_t pid, session_id, window_id;
    uint64_t handle;
    struct dfo_ticket ticket;
    bool committed;
};
struct nla_stage {
    struct dpr_service *processes;
    struct dfo_owner *frames;
    struct usr_runtime *sessions;
    struct nla_record entries[DVM_USER_SLOTS];
    bool ready;
};
struct nla_request {
    unsigned slot;
    uint32_t session_id, parent_pid;
    uint64_t hardware_context;
    const char *label, *window_title;
    struct ws_bounds bounds;
};
struct nla_result { uint32_t pid, window_id; struct dfo_ticket ticket; };

/* No new processes or sessions are fabricated by initialisation. */
enum nla_status nla_init(struct nla_stage *, struct dpr_service *,
                         struct dfo_owner *, struct usr_runtime *);
/* PRECONDITION (outside this function): proof of valid signed/approved .tea,
 * executable ELF64 mapped W^X, trusted context, true PA/page ownership and
 * safe kernel/device root closure. It is unsafe to call this from CPL3.
 * On partial failure, the committed record remains empty and the bound frame
 * lease is FAIL-CLOSED (revoked, scrubbed, not silently reused). */
enum nla_status nla_commit(struct nla_stage *, const struct nla_request *,
                           uint64_t observed_kernel_root,
                           struct nla_result *);
/* Invoke only after a trusted architecture backend proves context release,
 * absence of pending traps and all CPUs outside this root. Merely passing a
 * boolean/observed_root is NOT that proof. Process must be EXITED/FAULTED.
 * The terminal root is invalidated before the registry PID is reaped. */
enum nla_status nla_finalize(struct nla_stage *, unsigned slot,
                             uint64_t hardware_context,
                             uint64_t observed_kernel_root);

#endif
