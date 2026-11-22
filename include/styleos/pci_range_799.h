#ifndef STYLEOS_PCI_RANGE_799_H
#define STYLEOS_PCI_RANGE_799_H
#include <stdbool.h>
#include <stdint.h>
#include <styleos/pci_probe_794.h>
struct pci799_extent {uint64_t base, size, end;};
/* Pure bound decode. Caller must demonstrate quiescence and full PCI restore. */
bool pci799_decode(const struct pci794_bar *bar,uint32_t mask_lo,
                   uint32_t mask_hi,struct pci799_extent *out);
bool pci799_forwarded(const struct pci799_extent *extent,
                      uint64_t window_base,uint64_t window_end);
#endif
