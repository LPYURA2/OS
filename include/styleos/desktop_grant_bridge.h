#ifndef STYLEOS_DESKTOP_GRANT_BRIDGE_H
#define STYLEOS_DESKTOP_GRANT_BRIDGE_H

#include <styleos/frame_grant_proof.h>
#include <styleos/desktop_frame_owner.h>

/* 777: image-owned, single-CPU, *detached* page-root admission only.
 * This is not a PMM, complete kernel mapping or active CR3 switch.
 * The caller supplies a trusted real inherited-CR3 walker, checking
 * PA==VA, WB/present/write over ALL pages. A dummy callback is NOT proof.
 * This profile requires image-resident physical addresses identity mapped. */
#define DGB_SUPERVISOR_VA UINT64_C(0x00200000)
enum dgb_status {
    DGB_OK=0, DGB_ARGUMENT, DGB_POLICY, DGB_STORAGE, DGB_KERNEL,
    DGB_ATTEST, DGB_TABLES, DGB_READY
};
typedef bool (*dgb_attest)(void *ctx,uint64_t pa,uint64_t bytes);
struct dgb_result {
    uint64_t safe_root, user_root[DVM_USER_SLOTS];
    unsigned proved_pages;
};
/* The single supervisor diagnostic mapping is not a full kernel closure.
 * NONE of these roots may be activated. Owner/storage are kernel-owned.
 * All refusals before dfo_prepare leave owner/storage/output unchanged. */
enum dgb_status dgb_prepare(struct dfo_owner *,struct dvm_storage *,
    const struct boot_memory_evidence *,const struct fg_claims *,
    uint64_t supervisor_pa,dgb_attest,void *,struct dgb_result *);
#endif
