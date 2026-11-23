#ifndef STYLEOS_PCI_MCFG_813_H
#define STYLEOS_PCI_MCFG_813_H
#include <stdint.h>
#include <stddef.h>
#include <styleos/pci_requester_812.h>

#define PCI813_MAX_MCFG_BYTES 4096u
#define PCI813_MAX_WINDOWS 32u
#define PCI813_ECAM_BUS_BYTES UINT64_C(1048576)

/* A parsed MCFG window describes a firmware-claimed physical ECAM range,
 * not a usable mapping and not an IOMMU ownership/DMA grant. */
struct pci813_window {
 uint64_t base, physical_first, physical_end;
 uint16_t segment;
 uint8_t first_bus, last_bus;
};
struct pci813_mcfg {
 uint16_t count, segment0_windows, other_segment_windows;
 struct pci813_window windows[PCI813_MAX_WINDOWS];
};
struct pci813_live {
 uint16_t discovered_buses, covered_buses, missing_buses;
 uint16_t detected_functions, detected_bridges;
 uint64_t root_base, root_end;
};

/* Failures leave the caller's output unchanged. No table pointers retained. */
int pci813_mcfg_parse(const uint8_t *table,size_t size,struct pci813_mcfg *out);
/* 1=exactly one firmware allocation, 0=uncovered, -1=invalid input.
 * Writes address only when returning 1. Never dereference this address. */
int pci813_ecam_address(const struct pci813_mcfg *m,unsigned seg,unsigned bus,
 unsigned device,unsigned function,unsigned reg,uint64_t *address);
/* Covers ONLY the legacy-CF8-enumerated segment-zero topology. */
int pci813_correlate_bus0(const struct pci813_mcfg *m,
 const struct pci812_topology *t,struct pci813_live *out);
int pci813_allow_dma(const struct pci813_mcfg *m,unsigned seg,unsigned rid);
#endif
