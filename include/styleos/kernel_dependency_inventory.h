#ifndef STYLEOS_KERNEL_DEPENDENCY_INVENTORY_H
#define STYLEOS_KERNEL_DEPENDENCY_INVENTORY_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Read-only evidence. These bits describe absent proofs, not permissions. */
enum kdi_blocker {
    KDI_TSS_IST = 1u << 0,
    KDI_OTHER_STACKS = 1u << 1,
    KDI_FRAMEBUFFER = 1u << 2,
    KDI_DEVICE_MMIO = 1u << 3,
    KDI_FIRMWARE_RUNTIME = 1u << 4,
    KDI_WX_POLICY = 1u << 5,
    KDI_CACHE_POLICY = 1u << 6,
    KDI_PHYSICAL_OWNERSHIP = 1u << 7,
    KDI_PE_CONFLICT = 1u << 8
};
struct kdi_pe_profile {
    uint32_t image_pages, text_pages, readonly_pages, writable_pages;
    uint32_t mixed_pages, uncovered_pages, declared_extra_pages, sections;
};
/* Analyze *loaded* PE32+ headers/section RVAs, never executable file offsets.
 * No firmware call, allocation, relocation, or memory write is made. */
bool kdi_profile_loaded_pe(const void *loaded, size_t loaded_size,
                           struct kdi_pe_profile *profile);
struct kdi_live_dependencies {
    uint16_t tr_selector;
    uint32_t inherited_rwx_pages;
    bool tss_ist_proved, all_stacks_proved, framebuffer_proved;
    bool device_mmio_proved, firmware_runtime_proved;
    bool cache_policy_proved, kernel_wx_proved, physical_ownership_proved;
};
struct kdi_report {
    uint32_t blockers;
    /* This diagnostic deliberately cannot enable CR3, even with a synthetic
     * complete observation. Real activation requires a separate HW profile. */
    bool activation_permitted;
};
bool kdi_audit_dependencies(const struct kdi_pe_profile *pe,
                            const struct kdi_live_dependencies *deps,
                            struct kdi_report *out);
#endif
