#ifndef STYLEOS_DESKTOP_GOP_850_H
#define STYLEOS_DESKTOP_GOP_850_H
#include <styleos/apic_root_842.h>
#include <styleos/cache_pagewalk_801.h>
#include <styleos/desktop_display.h>
#include <styleos/pci_aperture_795.h>
/* Detached mapping proposal only. Trusted caller must bind all inputs to the
 * same boot, attest table reads and quiescent PCI ownership. No CR3 authority. */
struct s850_gop_request {
 const struct boot_info *boot;
 const struct boot_memory_evidence *memory;
 const struct desktop_surface *surface;
 const struct pci794_bar *bar;
 uint32_t mask_lo,mask_hi;
 const struct s842_cache *cache;
 uint64_t inherited_cr3;
 cache801_read_entry read_entry;
 void *read_context;
};
struct s850_gop_plan {
 struct pg_mapping mapping;
 uint64_t cache_bits, pages, bar_bytes;
 uint8_t pat_index;
 bool ready; /* proposal, never hardware permission */
};
bool s850_gop_prepare(const struct s850_gop_request *,struct s850_gop_plan *);
#endif
