#ifndef STYLEOS_PCI_ROOTBRIDGE_814_H
#define STYLEOS_PCI_ROOTBRIDGE_814_H
#include <stdint.h>
#include <stddef.h>
#include <styleos/pci_mcfg_813.h>
#define PCI814_MAX_ROOTS 16u
#define PCI814_MAX_DESCRIPTORS 24u
#define PCI814_MAX_RESOURCES 1200u
/* Copied UEFI PCI Root Bridge I/O Configuration resources.
 * NOT ACPI AML _CRS, not IOMMU/DMA authority. */
struct pci814_root {
 uint32_t segment;
 uint16_t bus_ranges,other_ranges,reserved;
 uint8_t bus_present[256];
};
struct pci814_report {
 uint16_t roots,segment0_roots,other_segment_roots,observed_buses,
 covered_buses,unresolved_buses,ranges,unknown_resource_descriptors;
};
int pci814_parse_configuration(uint32_t segment,const uint8_t *p,
 size_t length,struct pci814_root *out);
int pci814_correlate(const struct pci814_root *roots,size_t count,
 const struct pci813_mcfg *mcfg,const struct pci812_topology *topo,
 struct pci814_report *out);
int pci814_allow_dma(unsigned segment,unsigned requester_id);
#endif
