#ifndef STYLEOS_FRAME_GRANT_PROOF_H
#define STYLEOS_FRAME_GRANT_PROOF_H
#include <styleos/boot_memory.h>
#include <styleos/desktop_vm_stage.h>

/* Policy validation, NOT hardware proof or a transferable PMM grant. */
enum fg_status { FG_OK=0, FG_ARGUMENT, FG_LAYOUT, FG_NOT_IMAGE,
                 FG_NOT_RESERVED, FG_CANDIDATE, FG_CONFLICT };
struct fg_claims {
    uint64_t tables;
    uint64_t user[DVM_USER_SLOTS][DVM_USER_PAGES];
};
/* Caller must ALSO attest the actual inherited CR3 translations on hardware.
 * There is deliberately no bool flag that converts claims into trusted PAs. */
enum fg_status fg_check_policy(const struct boot_memory_evidence *,
                               const struct fg_claims *);
#ifdef STYLEOS_TEST_FRAME_GRANTS
struct pg_experiment;
void fg_experiment_run(struct pg_experiment *);
#endif
#endif
