#ifndef STYLEOS_DESKTOP_FRAME_OWNER_H
#define STYLEOS_DESKTOP_FRAME_OWNER_H

#include <styleos/desktop_vm_stage.h>
#include <styleos/process_runtime.h>

/* A bounded, kernel-private *lease ledger*, not a general physical allocator.
 * The caller must independently prove that every supplied physical frame is
 * reserved, mapped, kernel-owned and safe to use. No address is inferred from
 * a C pointer or a firmware candidate-RAM range. No CR3 writes. */
enum dfo_status {
    DFO_OK = 0, DFO_ARGUMENT, DFO_ALREADY_READY, DFO_NOT_READY,
    DFO_BAD_LAYOUT, DFO_BUSY, DFO_EVIDENCE, DFO_STALE, DFO_ACTIVE_ROOT,
    DFO_EXHAUSTED
};

struct dfo_claims {
    uint64_t table_pa;                       /* full 3*24-page arena */
    uint64_t user_pa[DVM_USER_SLOTS][DVM_USER_PAGES];
    size_t kernel_count;
    struct dvm_region kernel[DVM_KERNEL_MAX_SPANS];
};

struct dfo_ticket { uint32_t pid, generation; unsigned slot; };
struct dfo_lease {
    uint32_t pid, generation;
    uint64_t hardware_context;
    bool bound, revoked;
};

struct dfo_owner {
    struct dvm_storage *storage; /* lifetime owned by trusted kernel caller */
    struct dvm_output roots;
    struct dvm_plan plan;
    const struct pr_table *registry;
    struct dfo_lease lease[DVM_USER_SLOTS];
    uint32_t next_generation;
    bool ready;
};

/* Takes custody of an already-proven finite set of frames and builds the
 * detached SAFE, USER-A, USER-B roots transactionally. A failed preparation
 * never publishes a root and scrubs table storage. */
enum dfo_status dfo_prepare(struct dfo_owner *, struct dvm_storage *,
                            const struct dfo_claims *);

/* Bind only to an independently scheduler-admitted native user process with
 * the exact prepared page root. Does not launch or schedule that process. */
enum dfo_status dfo_bind(struct dfo_owner *, const struct pr_table *,
                         unsigned slot, uint32_t pid, uint64_t hardware_context,
                         struct dfo_ticket *);

/* Revocation needs a terminal registry state AND observation that the CPU is
 * no longer running the user's root. The architecture layer must also prove
 * no other CPU or pending trap references the context. It cannot be called
 * from user-mode code or used as a general kill/release syscall. */
enum dfo_status dfo_revoke(struct dfo_owner *, struct dfo_ticket,
                           uint64_t observed_cr3);

/* Scrubs the detached table arena after every lease has been revoked. The
 * kernel memory authority, not this function, must scrub/reclaim user data. */
enum dfo_status dfo_close(struct dfo_owner *, uint64_t observed_cr3);

#endif
