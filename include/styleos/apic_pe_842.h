#ifndef STYLEOS_APIC_PE_842_H
#define STYLEOS_APIC_PE_842_H
#include <styleos/apic_root_842.h>
#include <styleos/owned_low_790.h>
#include <styleos/acpi_madt_806.h>
/* Detached composition only. Raw hardware provenance remains a caller duty. */
bool s842_build_pe(const struct s790_low_request *pe,
 const struct boot_memory_map *map,const struct s842_cache *cache,
 const struct acpi806_topology *topology,uint64_t apic_msr,struct s842_root *out);
/* Live UEFI path: original descriptors, not conventional-memory list. */
bool s842_build_pe_efi(const struct s790_low_request *pe,
 const struct boot_memory_evidence *e,const struct boot_info *b,
 const struct s842_cache *cache,const struct acpi806_topology *topology,
 uint64_t apic_msr,struct s842_root *out);
#endif
