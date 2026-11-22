#ifndef STYLEOS_ACPI_MADT_806_H
#define STYLEOS_ACPI_MADT_806_H
#include <stdint.h>
#include <stddef.h>
/* Terminal diagnostic only. No firmware pointers are carried across EBS. */
#define ACPI806_MAX_ROOT 128u
#define ACPI806_MAX_IOAPIC 8u
#define ACPI806_MAX_ISO 16u
struct acpi806_ioapic {uint32_t id,physical,gsi_base;};
struct acpi806_override {uint32_t bus,source,gsi,flags;};
struct acpi806_topology {
 uint32_t revision,root_entries,madt_length,lapic_physical,madt_flags;
 uint32_t enabled_lapics,ioapic_count,iso_count;
 struct acpi806_ioapic ioapic[ACPI806_MAX_IOAPIC];
 struct acpi806_override iso[ACPI806_MAX_ISO];
};
/* Pure length-checked readers; caller must supply a readable buffer of the
   passed length. Direct firmware pointers require a trusted OVMF environment. */
int acpi806_rsdp(const uint8_t *rsdp,size_t supplied,uint64_t *xsdt);
int acpi806_xsdt(const uint8_t *root,size_t supplied,uint64_t *entries,size_t *count);
int acpi806_madt(const uint8_t *madt,size_t supplied,struct acpi806_topology *out);
#endif
