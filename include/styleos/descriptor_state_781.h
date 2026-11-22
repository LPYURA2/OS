#ifndef STYLEOS_DESCRIPTOR_STATE_781_H
#define STYLEOS_DESCRIPTOR_STATE_781_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Read-only observation of the current x86-64 IDT and descriptor selectors.
 * The caller MUST independently verify that all IDT pages are mapped and
 * readable under the inherited CR3 before passing any live IDT bytes.
 * A valid report NEVER grants access to a TSS/IST stack or permission to
 * activate a newly built CR3. */
struct dsi781_report {
    uint16_t cs, tr;
    uint32_t gates, ist_gates;
    uint8_t ist_index_mask;
    bool tss_descriptor_unresolved;
    bool tss_stack_unresolved;
    bool activation_permitted;
};

bool dsi781_audit_idt(const uint8_t *idt, size_t extent,
                      uint16_t cs, uint16_t tr, uint16_t gdt_limit,
                      struct dsi781_report *out);

/* A stack bound only proves the currently sampled stack pointer. It does
 * not prove other CPUs, IRQ stacks, TSS.RSP0 or any IST pointer. */
bool dsi781_current_rsp_in_boot_stack(uint64_t rsp,
                                      uint64_t boot_start,
                                      uint64_t boot_end);
#endif
