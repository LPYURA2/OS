#ifndef STYLEOS_DESKTOP_PROCESS_RUNTIME_H
#define STYLEOS_DESKTOP_PROCESS_RUNTIME_H

#include <styleos/process_api.h>
#include <styleos/process_scheduler.h>

/* Kernel-side desktop service; requires a trusted hardware context producer.
 * There is no CPL3 syscall, timer IRQ, .tea launcher or CPU%-sampling here. */
enum dpr_status {
    DPR_OK = 0, DPR_ARGUMENT, DPR_ALREADY_STARTED, DPR_NOT_STARTED,
    DPR_DIRTY, DPR_SCHEDULER, DPR_DENIED
};
struct dpr_service {
    struct ps_dispatcher dispatcher;
    struct ws_service *windows;
    bool started;
};

/* Initialization refuses non-empty registries/windows and never invents PIDs.
 * safe_root is a real, page-aligned root read by privileged arch code. */
enum dpr_status dpr_start(struct dpr_service *service,
                          struct pr_table *processes,
                          struct ws_service *windows,
                          uint64_t safe_root);
/* Process admission and events are for a trusted architecture backend ONLY.
 * A user app must never supply handles, roots, capability bits or trap proof. */
enum ps_status dpr_admit(struct dpr_service *service, unsigned slot,
                         uint64_t hardware_context, uint64_t user_root,
                         uint32_t parent_pid, const char *name,
                         uint32_t *pid);
enum ps_status dpr_dispatch(struct dpr_service *service,
                            unsigned *slot, uint32_t *pid);
enum ps_status dpr_observe(struct dpr_service *service, unsigned slot,
                           uint64_t hardware_context, uint64_t user_root,
                           uint64_t observed_safe_root, enum ps_event event);
/* Call only after arch backend has safely destroyed the user context. */
enum ps_status dpr_release(struct dpr_service *service, unsigned slot,
                           uint64_t hardware_context);
/* read-only kernel-internal snapshot, not a user pointer / syscall */
enum pa_status dpr_snapshot(struct dpr_service *service,
                            uint32_t kernel_authenticated_pid,
                            bool kernel_inspect_granted,
                            const struct pa_request *query,
                            struct pa_row *rows, size_t capacity,
                            struct pa_reply *reply);

/* Ordinary desktop boot-only singleton. Never reset while processes are alive. */
enum dpr_status dpr_global_start(uint64_t safe_root);
const struct dpr_service *dpr_global_state(void);
/* Architecture implementation for x86-64 desktop UEFI only. */
uint64_t dpr_arch_safe_root(void);

#endif
