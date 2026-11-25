#ifndef STYLEOS_RUNTIME_TICK_860_H
#define STYLEOS_RUNTIME_TICK_860_H
#include <styleos/desktop_process_runtime.h>
#include <styleos/process_entry_847.h>
/* Diagnostic integration contract only. No syscall, no user-supplied pointer.
 * The future architecture adapter must authenticate the actual IRQ frame,
 * ISR/EOI, saved full integer+FP context, owned roots and stack, and return
 * under the safe root with IF=0 before calling this single-CPU API.
 * A software receipt is not hardware evidence. No new scheduler state. */
/* IST top has the existing S847/S848 16-byte reserved tail. */
struct rt860_bounds {
    struct boot_range code, stack, ist;
    uint16_t cs, ss;
};
struct rt860_tick {
    unsigned slot, dispatch;
    uint32_t pid;
    uint64_t handle, child_root, safe_root, frame_va;
    uint64_t vector, isr, eoi, safe_flags;
    struct s847_raw frame;
};
/* Atomic registry update: tick count and READY state, no partial success. */
enum pr_status pr_tick_860(struct pr_table *,uint32_t,uint64_t,uint64_t);
/* Only a current dispatch ticket can consume one tick. */
enum ps_status ps_tick_860(struct ps_dispatcher *,const struct rt860_tick *);
/* Save the authenticated integer frame only after the existing scheduler
 * accepts the tick. FP storage is owned by the future architecture adapter.
 * Inputs and saved output are distinct supervisor-owned non-overlapping objects. */
enum ps_status dpr_tick_860(struct dpr_service *,const struct rt860_bounds *,
                          const struct rt860_tick *,struct s847_raw *saved);
/* Boot-only privileged accessor for the existing singleton. */
struct dpr_service *dpr_tick_global_860(void);
#endif
