#ifndef STYLEOS_TSS_IST_STAGE_782_H
#define STYLEOS_TSS_IST_STAGE_782_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <styleos/frame_grant_proof.h>

/* Stage 782 is a proposed, non-active long-mode TSS context. It does not
 * execute LGDT/LIDT/LTR, does not install IST gates and cannot grant CR3.
 * Every page is PE-owned and individually attested under the inherited CR3;
 * the inherited firmware PTEs are not independent W^X/guard mappings. */
#define TIS782_PAGE UINT64_C(4096)
#define TIS782_TSS_SIZE 104u
struct tis782_claims { uint64_t gdt, tss, rsp0, ist1; };
struct tis782_plan { uint64_t rsp0_top, ist1_top; bool staged, ltr_permitted; };
enum tis782_status { TIS782_OK=0, TIS782_ARGUMENT, TIS782_LAYOUT,
                     TIS782_CONFLICT, TIS782_NOT_RESERVED, TIS782_UNATTESTED };
typedef bool (*tis782_attest)(void *context, uint64_t va, uint64_t size);
enum tis782_status tis782_prepare(const struct boot_memory_evidence *e,
    const struct fg_claims *prior, const struct tis782_claims *claims,
    tis782_attest attest, void *context, struct tis782_plan *out);
/* Write a candidate TSS data image after successful staging only.
 * No GDT/TSS CPU descriptor is installed by this operation. */
bool tis782_encode(const struct tis782_plan *p, uint8_t *bytes, size_t size);
#endif
