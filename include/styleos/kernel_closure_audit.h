#ifndef STYLEOS_KERNEL_CLOSURE_AUDIT_H
#define STYLEOS_KERNEL_CLOSURE_AUDIT_H

#include <styleos/desktop_frame_owner.h>

/* 778: read-only, fail-closed audit of a *bounded* supervisor page inventory.
 * This is NOT proof that the kernel image, every stack, GDT/IDT/TSS, GOP/MMIO,
 * firmware handoff or all interrupt paths have been mapped. A KCA_OK result
 * NEVER authorizes MOV CR3. No activation API is exported here. */
enum kca_role {
    KCA_TEXT = 0, KCA_TRAP_TEXT, KCA_DATA, KCA_STACK,
    KCA_GDT, KCA_IDT, KCA_TSS, KCA_ROLE_COUNT
};
struct kca_page { uint64_t va, pa; };
struct kca_manifest { struct kca_page role[KCA_ROLE_COUNT]; };
enum kca_status {
    KCA_OK = 0, KCA_ARGUMENT, KCA_NOT_READY, KCA_LAYOUT, KCA_ALIAS,
    KCA_MISSING, KCA_TRANSLATION, KCA_RIGHTS, KCA_UNPROVEN
};
/* Must be a trusted architecture walker of the *active inherited* CR3 and
 * reservation map, not a caller-chosen boolean or a cast from a C pointer.
 * Host tests use a stub solely to exercise structural fail-closed behavior. */
typedef bool (*kca_attest)(void *context, uint64_t pa, uint64_t bytes);

enum kca_status kca_audit_core(const struct dfo_owner *owner,
    const struct kca_manifest *inventory, kca_attest attest, void *context);

#endif
