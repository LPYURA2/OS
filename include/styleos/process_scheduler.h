#ifndef STYLEOS_PROCESS_SCHEDULER_H
#define STYLEOS_PROCESS_SCHEDULER_H
#include <styleos/process_runtime.h>

/* Bounded cooperative dispatcher for the existing two-context user-mode
 * hardware demo. No timer IRQ, preemption or production desktop integration. */
/* Wide contract is currently host-only: the x86 hardware adapter still owns
 * exactly two roots, leases and FP contexts. Never enable this flag in an
 * existing guest image until the full architecture closure is generalized. */
#ifdef STYLEOS_KERNEL_CAPACITY_32
enum { PS_MAX_SLOTS = PR_MAX_PROCESSES };
#elif defined(STYLEOS_TEST_FOUR_GUEST_864)
enum { PS_MAX_SLOTS = 4 };
#elif defined(STYLEOS_TEST_WIDE_SCHEDULER_864)
enum { PS_MAX_SLOTS = PR_MAX_PROCESSES };
#else
enum { PS_MAX_SLOTS = 2 };
#endif
enum ps_status {
    PS_OK = 0, PS_ARGUMENT, PS_STATE, PS_FULL, PS_EMPTY, PS_EVIDENCE,
    PS_REGISTRY_ERROR, PS_EXHAUSTED
};
enum ps_event { PS_YIELD = 1, PS_EXIT = 2, PS_FAULT = 3 };
struct ps_slot { uint32_t pid; uint64_t handle, user_root; };
struct ps_dispatcher {
    struct pr_table *registry;
    uint64_t safe_root;
    struct ps_slot slots[PS_MAX_SLOTS];
    int current; /* -1 means kernel dispatcher */
    unsigned next, dispatches;
};
/* All arguments come from the trusted kernel, never an app request. */
enum ps_status ps_init(struct ps_dispatcher *, struct pr_table *, uint64_t safe_root);
enum ps_status ps_register(struct ps_dispatcher *, unsigned slot,
                           uint64_t hardware_context_handle, uint64_t user_root,
                           uint32_t parent_pid, const char *name,
                           uint32_t *new_pid);
enum ps_status ps_next(struct ps_dispatcher *, unsigned *slot, uint32_t *pid);
/* Call only AFTER verifying the exception/trap frame and restoring SAFE root.
 * Does not inspect the hardware itself; the architecture layer owns that proof. */
enum ps_status ps_accept(struct ps_dispatcher *, unsigned slot,
                         uint64_t hardware_context_handle,
                         uint64_t observed_user_root, uint64_t observed_safe_root,
                         enum ps_event event);
enum ps_status ps_reap(struct ps_dispatcher *, unsigned slot,
                       uint64_t hardware_context_handle);
#endif
