#ifndef STYLEOS_ACPI_HOSTBRIDGE_815_H
#define STYLEOS_ACPI_HOSTBRIDGE_815_H
#include <stddef.h>
#include <stdint.h>
#include <styleos/pci_mcfg_813.h>
#define ACPI815_MAX_DSDT 24576u
#define ACPI815_MAX_SSDT 64u
/* Bounded static AML candidates, never namespace evaluation or DMA grants. */
struct acpi815_report {
 uint32_t dsdt_length;
 uint16_t aml_device_packages,pci_hid_candidates,motherboard_hid_candidates;
 uint16_t seg_static_candidates,bbn_static_candidates,crs_static_candidates;
 uint16_t crs_method_candidates,ecam_literal_candidates,ecam_literal_matches;
 uint16_t unsupported_names,malformed_candidates,ssdt_count;
 uint8_t segment0_static_observed,bus0_static_observed;
 uint8_t acpi_namespace_evaluated,crs_evaluated,iommu_owned;
};
int acpi815_fadt_dsdt(const uint8_t *,size_t,uint64_t *);
int acpi815_inspect(const uint8_t *,size_t,const struct pci813_mcfg *,
 unsigned,struct acpi815_report *);
int acpi815_allow_dma(const struct acpi815_report *,unsigned);
#endif
