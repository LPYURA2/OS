#ifndef STYLEOS_RUNTIME_SELECT_862_H
#define STYLEOS_RUNTIME_SELECT_862_H
#include <styleos/process_scheduler.h>
#include <styleos/process_trap_847.h>
/* Supervisor-only candidate. The existing dispatcher has already selected
 * and committed PR_RUNNING. No independent scheduling choice. The adapter
 * still owns actual CPU/descriptor/FP/root admission and serialization. */
struct rt862_selection {
 unsigned slot, dispatch;
 uint32_t pid;
 uint64_t handle, child_root, safe_root;
};
/* No change to dispatcher, registry or output on refusal. The old context
 * model remains a transition checker; its cursor is not used or advanced. */
bool rt862_selected_context(struct s847_control *,const struct ps_dispatcher *,
 const struct rt862_selection *,uint64_t live_root,struct up_context *);
#endif
