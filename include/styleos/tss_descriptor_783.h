#ifndef STYLEOS_TSS_DESCRIPTOR_783_H
#define STYLEOS_TSS_DESCRIPTOR_783_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <styleos/tss_ist_stage_782.h>
/* Non-active GDT with verified 16-byte available x86-64 TSS descriptor.
 * Caller already attested old GDT under the inherited CR3 and target GDT,
 * TSS, RSP0 and IST1 PE-owned pages using the 781/782 hardware checks.
 * NO LGDT, LTR, IDT modification, CR3 activation or guard-page grant. */
#define TGD783_GDT_BYTES 4096u
struct tgd783_result {
    uint16_t tss_selector, new_gdt_limit;
    uint64_t tss_base, rsp0_top, ist1_top;
    bool descriptor_valid, active, guard_verified;
};
enum tgd783_status {
    TGD783_OK=0,TGD783_ARGUMENT,TGD783_GDT_BOUNDS,
    TGD783_CODE_DESCRIPTOR,TGD783_TSS_INVALID,TGD783_ALIAS
};
enum tgd783_status tgd783_build(const uint8_t *old_gdt,size_t old_bytes,
    uint16_t cs,uint16_t tr,uint64_t tss_base,
    const uint8_t *tss_bytes,size_t tss_size,const struct tis782_plan *stage,
    uint8_t *new_gdt,size_t new_bytes,struct tgd783_result *out);
bool tgd783_verify(const uint8_t *gdt,size_t len,
    const uint8_t *tss,size_t tss_size,const struct tgd783_result *r);
#endif
