#ifndef STYLEOS_DESKTOP_VM_STAGE_H
#define STYLEOS_DESKTOP_VM_STAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* x86-64, four-level, 4 KiB leaf tables only. This kernel-internal builder
 * has NO control-register writes and is NOT a userspace API. */
enum {
    DVM_PAGE_SIZE = 4096u,
    DVM_ROOTS = 3u,            /* 0 = SAFE, 1 and 2 = isolated user roots */
    DVM_USER_SLOTS = 2u,
    DVM_TABLES_PER_ROOT = 24u,
    DVM_KERNEL_MAX_SPANS = 4u,
    DVM_USER_PAGES = 3u       /* executable code, stack, private data */
};
#define DVM_USER_BEGIN UINT64_C(0x0000400000000000)
#define DVM_USER_END   UINT64_C(0x0000400000100000)

/* Present is implicit; no page can be simultaneously writable and executable.
 * Kernel mappings must remain supervisor-only; user mappings must live inside
 * the disjoint, finite user VA window. */
enum { DVM_WRITE = 1u, DVM_EXEC = 2u };
enum dvm_status {
    DVM_OK = 0, DVM_ARGUMENT, DVM_RANGE, DVM_ALIGNMENT, DVM_RIGHTS,
    DVM_ALIAS, DVM_CAPACITY, DVM_CORRUPT
};
struct dvm_region {
    uint64_t va;
    uint64_t pa;
    uint32_t pages;
    uint32_t rights;
};
struct dvm_plan {
    /* This does NOT prove ownership. Caller must first prove every PA/page
     * RESERVED/owned and the full kernel, interrupts and device closure. */
    uint64_t physical_table_base;
    size_t kernel_count;
    struct dvm_region kernel[DVM_KERNEL_MAX_SPANS];
    struct dvm_region user[DVM_USER_SLOTS][DVM_USER_PAGES];
};
struct dvm_storage {
    _Alignas(DVM_PAGE_SIZE)
    uint64_t table[DVM_ROOTS][DVM_TABLES_PER_ROOT][DVM_PAGE_SIZE / 8u];
};
struct dvm_output {
    uint64_t root[DVM_ROOTS];
    uint32_t used[DVM_ROOTS];
};
struct dvm_observed {
    uint64_t pa;
    bool user, writable, executable;
};

/* Builds an unactivated candidate. On any failure clears all candidate tables
 * and output roots. The physical_table_base is a proven PA supplied by the
 * privileged memory owner, NEVER inferred from a C pointer. */
enum dvm_status dvm_build(const struct dvm_plan *plan,
                          struct dvm_storage *storage,
                          struct dvm_output *output);
/* Structural walk of the candidate's own finite page-table arena; no arbitrary
 * physical-memory dereferencing. A 0 return means absent/invalid. */
bool dvm_lookup(const struct dvm_storage *storage,
                const struct dvm_output *output,
                unsigned root, uint64_t va,
                struct dvm_observed *found);

#endif
