#ifndef STYLEOS_PCI_PROBE_794_H
#define STYLEOS_PCI_PROBE_794_H
#include <stdint.h>
#include <stdbool.h>

/* PCI configuration mechanism #1: parsed read-only device header. 794 never
 * writes PCI configuration data or sizes BARs by temporarily disabling decode.
 * A BAR base alone CANNOT establish aperture size or safe cache attributes. */
struct pci794_bar {
    uint64_t base;
    uint32_t raw_lo, raw_hi;
    uint8_t index, width;
    bool memory, prefetch;
};
struct pci794_device {
    uint16_t vendor, device, command;
    uint8_t class_code, subclass, header_type;
    uint8_t bar_count;
    struct pci794_bar bars[6];
};
/* config is the sixteen naturally aligned 32-bit PCI config header words. */
bool pci794_parse_header(const uint32_t config[16], struct pci794_device *out);
bool pci794_find_framebuffer_bar(const struct pci794_device *device,
                                 uint64_t framebuffer_base,
                                 struct pci794_bar *out);
#endif
