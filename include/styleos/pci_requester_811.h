#ifndef STYLEOS_PCI_REQUESTER_811_H
#define STYLEOS_PCI_REQUESTER_811_H
#include <stdint.h>
#include <stddef.h>
#define PCI811_MAX_EXACT 256u
#define PCI811_MAX_UNITS 16u
/* An EXACT match is a proven table entry (not a DMA grant or PCI topology proof).
 * Multi-hop Intel paths and unsupported AMD entries remain unresolved. */
struct pci811_exact { uint16_t rid; uint8_t unit, kind; };
struct pci811_map {
 uint16_t units, exact_count, unresolved, include_all, ranges, collisions;
 struct pci811_exact entries[PCI811_MAX_EXACT];
};
int pci811_dmar_decode(const uint8_t *p,size_t len,struct pci811_map *out);
int pci811_ivrs_decode(const uint8_t *p,size_t len,struct pci811_map *out);
/* 1: one explicit table owner; 0: unresolved; -1: conflicting owner. */
int pci811_lookup(const struct pci811_map *m,uint16_t rid,uint8_t *unit);
/* Never a DMA authorization regardless of decoding results. */
int pci811_allow_dma(const struct pci811_map *m,uint16_t rid);
#endif
