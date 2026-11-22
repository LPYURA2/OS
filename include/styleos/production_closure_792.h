#ifndef STYLEOS_PRODUCTION_CLOSURE_792_H
#define STYLEOS_PRODUCTION_CLOSURE_792_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 792 is a read-only inventory contract, never a CR3 enablement API. */
enum pc792_role {
    PC792_IMAGE_RX, PC792_IMAGE_RO, PC792_IMAGE_RW,
    PC792_BOOT_STACK, PC792_IST_STACKS, PC792_DESCRIPTORS,
    PC792_PAGE_TABLES, PC792_BOOT_MAP, PC792_GOP_FRAMEBUFFER,
    PC792_DEVICE_MMIO, PC792_IO_PORTS, PC792_INTERRUPT_STATE,
    PC792_FIRMWARE_POLICY, PC792_ROLE_COUNT
};
enum pc792_kind { PC792_RAM=1, PC792_FRAMEBUFFER=2, PC792_MMIO=3,
                  PC792_IOPORT=4, PC792_ASSERTION=5 };
enum pc792_cache { PC792_CACHE_UNKNOWN, PC792_WB, PC792_WC, PC792_UC };
enum pc792_evidence { PC792_UNPROVEN, PC792_REPORTED,
                      PC792_ATTESTED };
struct pc792_item {
    enum pc792_role role;
    enum pc792_kind kind;
    enum pc792_cache cache;
    enum pc792_evidence evidence;
    uint64_t start, length;
    bool writable, executable, user;
};
#define PC792_MAX_ITEMS 32u
#define PC792_REQUIRED_MASK ((UINT32_C(1) << PC792_ROLE_COUNT)-1u)
struct pc792_report {
    uint32_t observed_mask;
    uint32_t attested_mask;
    uint32_t missing_mask;
    uint32_t unverified_mask;
    uint32_t conflicts;
    bool activation_permitted; /* ALWAYS false, including synthetic "complete" */
};
enum pc792_status {
    PC792_OK, PC792_ARGUMENT, PC792_RANGE, PC792_RIGHTS,
    PC792_CACHE_POLICY, PC792_DUPLICATE, PC792_OVERLAP
};
/* Caller-supplied attestations are assertions until separately hardware-bound.
 * This preflight checks structure and reports evidence gaps but never activates
 * hardware or treats a reported claim as proof. All results are read-only. */
enum pc792_status pc792_inventory(const struct pc792_item *items,
                                  size_t count,struct pc792_report *out);
const char *pc792_role_name(enum pc792_role role);
#endif
