#include <styleos/irq_apic_805.h>
#include <stddef.h>
#include <stdint.h>
/* Q35/APIC local CPU-state admissibility, NOT I/O APIC/MADT validation.
 * No writes or pointer dereferences. A broad hardware topology is not proven. */
bool irq805_validate(const struct irq805_sample *s, uint64_t *lapic_phys) {
    if (!s || !lapic_phys) return false;
    *lapic_phys = 0;
    if (!(s->cpuid_edx & (UINT32_C(1)<<9))) return false; /* APIC */
    if (!(s->apic_base & (UINT64_C(1)<<11))) return false; /* global enable */
    if ((s->apic_base & (UINT64_C(1)<<10)) &&
        !(s->cpuid_ecx & (UINT32_C(1)<<21))) return false; /* x2APIC */
    if ((s->rflags & (UINT64_C(1)<<9)) != 0) return false; /* IF after cli */
    if (!s->idtr_base || s->idtr_limit < 15 || !(s->cr3 & ~UINT64_C(0xfff))) return false;
    /* APIC_BASE address is page-aligned; non-address low bits may be set. */
    const uint64_t base = s->apic_base & UINT64_C(0x000ffffffffff000);
    if (!base || base >= (UINT64_C(1)<<40)) return false;
    *lapic_phys=base;
    return true;
}
