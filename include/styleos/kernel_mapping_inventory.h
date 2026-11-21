#ifndef STYLEOS_KERNEL_MAPPING_INVENTORY_H
#define STYLEOS_KERNEL_MAPPING_INVENTORY_H
#include <stdbool.h>
#include <stdint.h>
/* 779: a read-only accounting contract for mappings observed by a trusted
 * architecture walker under the inherited active CR3.  This is never a grant
 * of physical pages, and it does NOT authorize activating detached roots. */
enum kmi_kind { KMI_IMAGE, KMI_BOOT_STACK, KMI_GDT, KMI_IDT, KMI_OLD_TABLE,
                KMI_KIND_COUNT };
enum kmi_error { KMI_OK, KMI_ARGUMENT, KMI_PAGE, KMI_TRANSLATION,
                 KMI_PRIVILEGE, KMI_CACHE, KMI_LIMIT };
#define KMI_MAX_ACCOUNTED 16384u
struct kmi_observation {
    uint64_t va, pa;
    unsigned cache_type;
    bool writable, executable, user;
};
struct kmi_inventory {
    uint32_t pages[KMI_KIND_COUNT];
    uint32_t rwx_pages;
    uint32_t checked_pages;
    /* No caller may infer from these counters that TSS, IST, runtime firmware,
     * framebuffer or MMIO have all been discovered and mapped. */
    bool root_activation_permitted;
};
enum kmi_error kmi_account(struct kmi_inventory *, enum kmi_kind,
                           const struct kmi_observation *, bool identity);
#endif
