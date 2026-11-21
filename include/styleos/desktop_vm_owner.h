#ifndef STYLEOS_DESKTOP_VM_OWNER_H
#define STYLEOS_DESKTOP_VM_OWNER_H

#include <styleos/desktop_vm_stage.h>
#include <styleos/boot_memory.h>

/* Reserved-image arena discipline for the two fixed DVM user contexts.
 * Internal x86-64 kernel staging ONLY. No allocation from UEFI candidates,
 * no syscall, no CR3 writes, no arbitrary page grant. */
enum {
    DVO_TABLE_PAGES = DVM_ROOTS * DVM_TABLES_PER_ROOT,
    DVO_OWNED_PAGES = DVO_TABLE_PAGES + DVM_USER_SLOTS * DVM_USER_PAGES,
    DVO_OWNED_BYTES = DVO_OWNED_PAGES * DVM_PAGE_SIZE
};

/* Witness must be a trusted, read-only walk of the ACTUAL inherited root.
 * true only if VA resolves to expected PA, writable, supervisor-only, WB.
 * Reject if the walker cannot safely access its own inherited table pages. */
typedef bool (*dvo_identity_witness)(void *context, uint64_t va,
                                     uint64_t expected_pa);

struct dvo_arena {
    uint64_t base; /* fixed image-backed identity VA == PA (after witness) */
    uint64_t bytes;
};

struct dvo_exclusions {
    struct boot_range pool, pool_metadata; /* existing bootstrap pool */
};

enum dvo_status {
    DVO_OK = 0, DVO_ARGUMENT, DVO_ALIGNMENT, DVO_SCOPE,
    DVO_RESERVED, DVO_OVERLAP, DVO_LAYOUT, DVO_PROOF, DVO_BUILD
};

/* The storage pointer MUST be exactly the arena base. The arena is a
 * dedicated, non-reusable image-owned symbol declared by a trusted caller;
 * input checks alone are NOT ownership of arbitrary pages. */
enum dvo_status dvo_build_owned(const struct boot_memory_evidence *evidence,
                                struct dvo_arena arena,
                                struct dvo_exclusions exclude,
                                const struct dvm_plan *plan,
                                struct dvm_storage *storage,
                                struct dvm_output *output,
                                dvo_identity_witness witness,
                                void *witness_context);

#endif
