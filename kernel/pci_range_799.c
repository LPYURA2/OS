#include <styleos/pci_range_799.h>
#include <stddef.h>
#include <stdint.h>
bool pci799_decode(const struct pci794_bar *bar,uint32_t mask_lo,
                   uint32_t mask_hi,struct pci799_extent *out)
{
    if (!bar || !out || !bar->memory || !bar->base || bar->index>=6 ||
        (bar->width!=32u && bar->width!=64u) ||
        (bar->width==64u && bar->index>=5u) || (bar->raw_lo&1u) || (((bar->raw_lo&8u)!=0u)!=bar->prefetch) ||
        (bar->raw_lo&15u)!=(mask_lo&15u)) return false;
    uint64_t m,bytes;
    if (bar->width==32u) {
        if (bar->raw_hi || mask_hi || (bar->raw_lo&6u)) return false;
        m=(uint64_t)(mask_lo&UINT32_C(0xfffffff0));
        bytes=(UINT64_C(1)<<32)-m;
    } else {
        if ((bar->raw_lo&6u)!=4u) return false;
        m=((uint64_t)mask_hi<<32)|(uint64_t)(mask_lo&UINT32_C(0xfffffff0));
        bytes=(~m)+1u;
    }
    if (bytes<4096u || (bytes&(bytes-1u)) ||
        bar->base%bytes || bar->base>= (UINT64_C(1)<<52) ||
        bytes>(UINT64_C(1)<<52) || bar->base>UINT64_MAX-bytes) return false;
    *out=(struct pci799_extent){bar->base,bytes,bar->base+bytes};
    return true;
}
bool pci799_forwarded(const struct pci799_extent *extent,uint64_t start,uint64_t end)
{
    return extent && start<end && start<=extent->base &&
           extent->base<extent->end && extent->end<=end;
}
