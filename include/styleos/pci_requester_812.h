#ifndef STYLEOS_PCI_REQUESTER_812_H
#define STYLEOS_PCI_REQUESTER_812_H
#include <stdint.h>
#include <stddef.h>
#define PCI812_MAX_EXACT 256u
#define PCI812_MAX_ALIASES 256u
#define PCI812_MAX_FUNCTIONS 4096u
#define PCI812_MAX_UNITS 16u
/* Discovery is never an ownership or DMA-grant assertion. */
struct pci812_topology {
 uint8_t kind[65536], next_bus[65536], seen[256], upper[256];
 uint16_t buses, functions, bridges;
};
struct pci812_entry { uint16_t rid; uint8_t unit, source; };
struct pci812_alias { uint16_t from, to; uint8_t unit; };
struct pci812_audit {
 uint16_t units, exact, unresolved, include_all, ranges, bridge_scopes;
 uint16_t aliases, extended, special, exceptions, nonzero_segments, collisions;
 struct pci812_entry records[PCI812_MAX_EXACT];
 struct pci812_alias alias[PCI812_MAX_ALIASES];
};
void pci812_topology_init(struct pci812_topology *t);
int pci812_add_function(struct pci812_topology *t,unsigned bus,unsigned device,
 unsigned function,unsigned kind,unsigned secondary,unsigned subordinate);
int pci812_resolve(const struct pci812_topology *t,unsigned start_bus,
 const uint8_t *pairs,size_t count,unsigned scope_kind,uint16_t *rid);
int pci812_dmar_decode(const uint8_t *p,size_t len,
 const struct pci812_topology *t,struct pci812_audit *out);
int pci812_ivrs_decode(const uint8_t *p,size_t len,
 const struct pci812_topology *t,struct pci812_audit *out);
int pci812_lookup(const struct pci812_audit *a,uint16_t rid,uint8_t *unit);
int pci812_allow_dma(const struct pci812_audit *a,uint16_t rid);
#endif
