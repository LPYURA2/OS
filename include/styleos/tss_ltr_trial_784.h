#ifndef STYLEOS_TSS_LTR_TRIAL_784_H
#define STYLEOS_TSS_LTR_TRIAL_784_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <styleos/tss_descriptor_783.h>

/* A terminal, dedicated QEMU-only machine-state transition. Never call this
 * in Desktop or in a context expected to return to UEFI. This is NOT an IST
 * delivery/guard-page test and does not authorize a new CR3 or runtime TSS. */
struct tlt784_context {
    uint64_t old_cr3, gdt_base, idt_base;
    uint16_t old_limit, idt_limit, cs, tr;
    bool interrupts_disabled, idt_mapped, pages_attested;
};
enum tlt784_status {
    TLT784_OK = 0, TLT784_BAD_CONTEXT, TLT784_BAD_DESCRIPTOR,
    TLT784_BAD_HANDLERS
};
enum tlt784_status tlt784_preflight(const struct tlt784_context *ctx,
    const struct tgd783_result *descriptor,
    const uint8_t *new_gdt, size_t gdt_bytes,
    const uint8_t *tss, size_t tss_bytes,
    const uint8_t *active_idt, size_t idt_bytes);
#ifdef STYLEOS_TEST_TSS_LTR_784
struct pg_experiment;
_Noreturn void tlt784_terminal_trial(struct pg_experiment *paging,
    const struct tgd783_result *descriptor,
    uint8_t *new_gdt, const uint8_t *tss);
#endif
#endif
