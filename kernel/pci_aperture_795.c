#include <styleos/pci_aperture_795.h>
#include <stddef.h>
#include <stdint.h>

bool pci795_decode_aperture(const struct pci794_bar *bar, uint32_t mask_lo,
                            uint32_t mask_hi, uint64_t fb_base,
                            uint64_t fb_bytes, struct pci795_aperture *out)
{
    if (!bar || !out || !bar->memory || !bar->base ||
        (bar->width != 32u && bar->width != 64u) ||
        bar->index > (bar->width == 64u ? 4u : 5u) ||
        (bar->raw_lo & 1u) || (bar->raw_lo & 0x0fu) != (mask_lo & 0x0fu) ||
        fb_base != bar->base || !fb_bytes || fb_base > UINT64_MAX - fb_bytes)
        return false;
    uint64_t bytes;
    if (bar->width == 32u) {
        if (mask_hi || bar->raw_hi || (bar->raw_lo & 6u)) return false;
        uint32_t bitmask = mask_lo & UINT32_C(0xfffffff0);
        bytes = UINT64_C(0x100000000) - (uint64_t)bitmask;
    } else {
        if ((bar->raw_lo & 6u) != 4u) return false;
        uint64_t bitmask = ((uint64_t)mask_hi << 32) |
                           (uint64_t)(mask_lo & UINT32_C(0xfffffff0));
        bytes = (~bitmask) + UINT64_C(1);
    }
    if (bytes < 4096u || (bytes & (bytes - 1u)) ||
        bar->base % bytes || bar->base > UINT64_MAX - bytes ||
        bar->base >= (UINT64_C(1) << 52) ||
        bytes > (UINT64_C(1) << 52) ||
        fb_base + fb_bytes > bar->base + bytes)
        return false;
    *out = (struct pci795_aperture){bar->base, bytes, bar->base + bytes, true};
    return true;
}
