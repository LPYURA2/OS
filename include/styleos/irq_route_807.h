#ifndef STYLEOS_IRQ_ROUTE_807_H
#define STYLEOS_IRQ_ROUTE_807_H
#include <styleos/acpi_madt_806.h>
#include <stdint.h>
/* A bounded, topology-derived ISA IRQ0 route. The IOAPIC version register
 * supplies the real pin count; never infer a pin count from MADT alone. */
struct irq807_plan {uint32_t source_irq,gsi,ioapic_index,pin,polarity,trigger;};
int irq807_select_pit(const struct acpi806_topology *t,
                      const uint32_t redir_counts[ACPI806_MAX_IOAPIC],
                      struct irq807_plan *out);
#endif
