#ifndef STYLEOS_PCI_APERTURE_795_H
#define STYLEOS_PCI_APERTURE_795_H
#include <stdbool.h>
#include <stdint.h>
#include <styleos/pci_probe_794.h>

/* Decode a standard PCI BAR sizing-mask readback. This helper is pure: it
 * never touches PCI configuration registers or assigns memory mappings. */
struct pci795_aperture {
    uint64_t base;
    uint64_t bytes;
    uint64_t end;
    bool framebuffer_covered;
};
bool pci795_decode_aperture(const struct pci794_bar *bar, uint32_t mask_lo,
                            uint32_t mask_hi, uint64_t framebuffer_base,
                            uint64_t framebuffer_bytes,
                            struct pci795_aperture *result);
#endif
