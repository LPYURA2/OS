#ifndef STYLEOS_OWNERSHIP_H
#define STYLEOS_OWNERSHIP_H
#include "styleos/boot_memory.h"
#define OP_PAGE UINT64_C(4096)
#define OP_WORKSPACE_BYTES 65536u
#define OP_CLAIMS_MAX 96u
#define OP_RANGES_MAX 512u
#define OP_TABLES_MAX 64u
#define OP_WALKS_MAX (OP_TABLES_MAX + 2u)
#define OP_DIAG_BYTES (sizeof(struct boot_memory_map) + sizeof(struct boot_memory_evidence))
enum op_source { OP_UEFI=1, OP_FDT=2 };
enum op_phase { OP_INHERITED=1, OP_MMU_OFF=2 };
enum op_kind { OP_IMAGE=1, OP_STACK, OP_HANDOFF, OP_SOURCE_BUFFER,
    OP_MAP_WORKSPACE, OP_OWN_WORKSPACE, OP_BOOT_POOL, OP_POOL_METADATA,
    OP_GDT, OP_IDT, OP_ROOT, OP_OLD_TABLE, OP_VECTORS, OP_MMIO };
enum op_proof { OP_UNPROVEN, OP_IMAGE_IDENTITY, OP_BOOT_SOURCE,
    OP_OLD_WALK, OP_REGISTER, OP_FIXED_DEVICE };
enum op_unresolved { OP_TABLE_SET_INCOMPLETE=1, OP_OTHER_FIRMWARE_STATE=2 };
enum op_error { OP_OK, OP_ARGUMENT, OP_RANGE, OP_ALIGNMENT, OP_OVERFLOW,
    OP_LIMIT, OP_MAP, OP_SOURCE, OP_REQUIRED, OP_PROOF, OP_PLATFORM, OP_CONTEXT };
struct op_claim { uint32_t kind, proof; uint64_t va, pa, bytes; };
struct op_snapshot {
    uint64_t mode, root, aux0, aux1; /* x86 CR0/CR3/CR4/EFER; ARM SCTLR/0/VBAR/DAIF. */
    uint64_t pat, rflags; /* Actual inherited x86 state; zero on ARM. */
    uint64_t gdt_va, gdt_bytes, idt_va, idt_bytes, cs;
    uint32_t pa_bits, phase, features; /* x86 bits0=NX,1=1GiB; ARM zero. */
};
enum op_walk_kind { OP_WALK_GDT=1, OP_WALK_TABLE=2 };
struct op_walk {
    uint32_t kind, levels;
    uint64_t va, pa, tables[4], entries[4];
};
struct op_input {
    uint32_t arch, source, unresolved;
    const struct boot_memory_map *map;
    struct boot_memory_live live;
    struct boot_range map_workspace, own_workspace, pool, pool_metadata;
    uint64_t source_stride, source_version;
    struct op_snapshot snapshot;
    struct op_claim claims[OP_CLAIMS_MAX];
    size_t nr_claims;
    struct op_walk walks[OP_WALKS_MAX];
    size_t nr_walks;
};
struct op_output {
    struct boot_range protected_ranges[OP_RANGES_MAX], review[OP_RANGES_MAX];
    size_t nr_protected, nr_review;
    uint64_t protected_pages, review_pages, granted;
    int valid; /* Structural validity only. Never an ownership grant. */
};
struct op_scratch {
    struct boot_memory_map checked;
    struct boot_range ranges[BOOT_MEMORY_INPUT_MAX + OP_CLAIMS_MAX];
};
struct op_state { struct op_input input; struct op_output output; struct op_scratch scratch;
    struct op_snapshot after; };
union op_workspace { struct op_state state; unsigned char bytes[OP_WORKSPACE_BYTES]; };
extern union op_workspace styleos_ownership_workspace;

enum op_error op_add_claim(struct op_input *, enum op_kind, enum op_proof,
                          uint64_t va, uint64_t pa, uint64_t bytes);
enum op_error op_build(const struct op_input *, struct op_output *, struct op_scratch *);
const char *op_error_name(enum op_error);
const char *op_kind_name(unsigned);
const char *op_proof_name(unsigned);
enum op_error op_arch_collect(struct op_input *, const struct boot_info *);
int op_arch_unchanged(const struct op_input *, struct op_snapshot *after);
void ownership_plan_run(const struct boot_info *, const struct boot_memory_evidence *);
#endif
