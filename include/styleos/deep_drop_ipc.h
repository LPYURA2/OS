#ifndef STYLEOS_DEEP_DROP_IPC_H
#define STYLEOS_DEEP_DROP_IPC_H
#include <styleos/deep_drop.h>

struct dd_gesture {
    struct dd_endpoint source, target;
    uint64_t id, deadline;
};
struct dd_ipc {
    struct dd_service broker;
    struct dd_gesture gesture;
    uint64_t last_gesture, last_time;
};
/* Zero-initialize once. Only the trusted compositor selects these windows. */
enum dd_status dd_ipc_gesture(struct dd_ipc *,uint32_t source,uint32_t target,
                              uint64_t now,uint64_t lifetime);
/* Trusted release over an authenticated panel token. Restore a minimized
 * destination before publishing the ordinary gesture. On refusal restore its
 * previous visibility. No privilege for hidden/foreign/stale windows. */
enum dd_status dd_ipc_task_gesture(struct dd_ipc *,uint32_t source,const struct nwc_task *target,
                                   uint64_t now,uint64_t lifetime);
/* The trap supplies the caller identity and an already audited private alias.
 * All calls run serialized on the safe root with interrupts disabled. */
enum dd_status dd_ipc_call(struct dd_ipc *,struct dd_endpoint caller,unsigned op,
                           uint64_t id,uint8_t *span,size_t size,uint64_t now,
                           uint64_t *result);
bool dd_ipc_reconcile(struct dd_ipc *,uint64_t now);
#endif
