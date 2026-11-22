#ifndef STYLEOS_IRQ_APIC_805_H
#define STYLEOS_IRQ_APIC_805_H
#include <stdint.h>
#include <stdbool.h>
/* Read-only post-EBS status; NOT an IRQ router or a delivery proof. */
struct irq805_sample {
    uint32_t cpuid_ecx, cpuid_edx;
    uint64_t apic_base, idtr_base, rflags, cr3;
    uint16_t idtr_limit;
    uint8_t pic_master, pic_slave;
};
bool irq805_validate(const struct irq805_sample *s, uint64_t *lapic_phys);
#endif
