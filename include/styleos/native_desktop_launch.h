#ifndef STYLEOS_NATIVE_DESKTOP_LAUNCH_H
#define STYLEOS_NATIVE_DESKTOP_LAUNCH_H

#include <styleos/native_launch_stage.h>
#include <styleos/native_window_channel.h>

/* Stage 775: single-CPU, kernel-only pre-dispatch admission coordinator.
 * Does not authorize packages, allocate physical RAM, authenticate accounts,
 * switch CR3, trap into CPL3, or turn a UI session number into a PID.
 * All external architecture/package/memory evidence required by nla_commit
 * must already be established by the trusted caller. */
enum ndl_status {
    NDL_OK = 0, NDL_ARGUMENT, NDL_NOT_READY, NDL_CONFLICT,
    NDL_LAUNCH_DENIED, NDL_PORT_DENIED, NDL_ROLLBACK_FAILED,
    NDL_STALE, NDL_NOT_TERMINATED
};
struct ndl_record {
    uint32_t pid, window_id;
    uint64_t hardware_context;
    bool active;
};
struct ndl_stage {
    struct nla_stage *launch;
    struct nwc_service *channel;
    struct ndl_record records[DVM_USER_SLOTS];
    bool ready;
};
struct ndl_result {
    uint32_t pid, window_id;
    struct dfo_ticket lease;
};

/* Must be invoked before any native process has been admitted. It does not
 * create a user, process, window or IPC port. */
enum ndl_status ndl_init(struct ndl_stage *, struct nla_stage *,
                         struct nwc_service *);
/* Precondition: scheduler quiescent and still holding the trusted kernel root.
 * Admission and port claim form ONE operation. On port-claim failure, the
 * still-unrun READY process is marked terminal, windows removed and the
 * detached page root revoked/scrubbed. It cannot be retried with old frames.
 * Never invoke for a context that has already executed user instructions. */
enum ndl_status ndl_admit(struct ndl_stage *, const struct nla_request *,
                          uint64_t observed_safe_root, struct ndl_result *);
/* Only after trusted architecture teardown of every CPU, user stack and trap
 * frame. A terminal registry state alone is not proof of safe memory reclaim.
 * Removes the lease + process and drops its IPC port before returning. */
enum ndl_status ndl_reap(struct ndl_stage *, unsigned slot,
                         uint64_t hardware_context, uint64_t observed_safe_root);
/* Reclaim ports of windows/processes already invalidated by the kernel.
 * Has no authority to terminate a process or free any physical pages. */
size_t ndl_reconcile_ports(struct ndl_stage *);

#endif
