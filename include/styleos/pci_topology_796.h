#ifndef STYLEOS_PCI_TOPOLOGY_796_H
#define STYLEOS_PCI_TOPOLOGY_796_H
#include <stdbool.h>
#include <stdint.h>
/* Read-only PCI type-1 bridge decoding. Never assume a BAR aperture size or
 * grant a resource from a config register. The window is only an observation. */
struct pci796_bridge {
    uint8_t primary, secondary, subordinate;
    uint64_t memory_base, memory_end; /* end exclusive, zero/zero if disabled */
    uint64_t prefetch_base, prefetch_end;
    bool memory_window, prefetch_window;
};
/* 16 dwords: common PCI config header including type-1 bridge fields. */
bool pci796_parse_bridge(const uint32_t words[16], uint8_t observed_bus,
                         struct pci796_bridge *out);
/* A bus can be enqueued once. Repeated routes are ambiguous, not silently
 * followed. All arrays are fixed and bounded; no heap or production driver. */
bool pci796_queue_secondary(uint8_t secondary, uint8_t queue[256],
                            uint16_t *tail, bool visited[256]);
#endif
