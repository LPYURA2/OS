#ifndef STYLEOS_IST_GUARD_PLAN_785_H
#define STYLEOS_IST_GUARD_PLAN_785_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <styleos/boot_memory.h>

/* Stage 785 structural-only plan. No CR3, IDT, TSS, or hardware state writes.
 * Cannot be treated as an ownership grant or a hardware stack-guard proof. */
#define IGP785_PAGE UINT64_C(4096)
#define IGP785_IST_ALIAS UINT64_C(0x0000400000003000)
#define IGP785_FRAMES 9u
#define IGP785_MAX_EXCLUSIONS 32u
#define IGP785_ENTRIES 512u
#define IGP785_STACK_PTE (UINT64_C(0x8000000000000003))

enum igp785_frame {
    IGP785_NEW_PML4=0, IGP785_NEW_PDPT, IGP785_NEW_PD, IGP785_NEW_PT,
    IGP785_NEW_IDT, IGP785_GDT, IGP785_TSS, IGP785_RSP0, IGP785_IST1
};
struct igp785_request {
    struct boot_range reserved_image;
    struct boot_range candidate_ram[IGP785_MAX_EXCLUSIONS];
    size_t nr_candidates;
    struct boot_range protected_ranges[IGP785_MAX_EXCLUSIONS];
    size_t nr_protected;
    uint64_t old_cr3_pa;
    uint64_t pages[IGP785_FRAMES];
    uint64_t virtual_ist;
};
struct igp785_layout {
    uint64_t virtual_guard_below, virtual_ist, virtual_guard_above;
    uint64_t virtual_ist_top;
    uint64_t new_root_pa;
    bool structural_only;
    bool hardware_guard_proven;
    bool hardware_ist_proven;
    bool activation_allowed;
};
enum igp785_error {
    IGP785_OK=0, IGP785_INVALID, IGP785_OUTSIDE_IMAGE,
    IGP785_ALIAS, IGP785_COLLISION, IGP785_EXCLUDED,
    IGP785_OCCUPIED, IGP785_TABLE_INVALID
};
enum igp785_error igp785_prepare_layout(const struct igp785_request *req,
    struct igp785_layout *out);
/* Pure staging buffers for one *inactive* 4-level hierarchy. The PML4 is
 * copied from a supplied read-only inherited root. Entry 128 MUST be zero;
 * no firmware mapping is overwritten. Only the IST alias has a leaf, with
 * two absent guard neighbors. These arrays are not installed by this API. */
struct igp785_tables {
    uint64_t pml4[IGP785_ENTRIES];
    uint64_t pdpt[IGP785_ENTRIES];
    uint64_t pd[IGP785_ENTRIES];
    uint64_t pt[IGP785_ENTRIES];
};
enum igp785_error igp785_prepare_tables(const struct igp785_request *req,
    const uint64_t old_pml4[IGP785_ENTRIES],
    struct igp785_tables *tables, struct igp785_layout *layout);
bool igp785_audit_tables(const struct igp785_request *req,
    const uint64_t old_pml4[IGP785_ENTRIES],
    const struct igp785_tables *tables, const struct igp785_layout *layout);
#endif
