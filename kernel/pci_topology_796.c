#include <styleos/pci_topology_796.h>
#include <stddef.h>

bool pci796_parse_bridge(const uint32_t w[16], uint8_t observed_bus,
                         struct pci796_bridge *out)
{
    if (!w || !out) return false;
    *out=(struct pci796_bridge){0};
    uint16_t vendor=(uint16_t)w[0];
    if (vendor==0 || vendor==0xffffu || ((w[3]>>16)&0x7fu)!=1u ||
        (w[2]>>24)!=6u || ((w[2]>>16)&0xffu)!=4u) return false;
    uint8_t primary=(uint8_t)w[6];
    uint8_t secondary=(uint8_t)(w[6]>>8);
    uint8_t subordinate=(uint8_t)(w[6]>>16);
    if (primary!=observed_bus || !secondary || secondary==primary ||
        subordinate<secondary) return false;
    out->primary=primary;out->secondary=secondary;out->subordinate=subordinate;
    /* Conventional non-prefetchable PCI bridge memory window, 1MiB units.
     * Disabled windows have base > limit and MUST remain absent. */
    uint32_t mb=(w[8]&0x0000fff0u)<<16;
    uint32_t ml=(w[8]&0xfff00000u)|0xfffffu;
    if (mb<=ml) {
        out->memory_window=true;
        out->memory_base=mb;
        out->memory_end=(uint64_t)ml+1u;
    }
    /* Prefetchable memory window can be either 32-bit or 64-bit. 64-bit
     * upper registers are at offset 0x28/0x2c (dwords 10/11). */
    uint32_t pb=(w[9]&0x0000fff0u)<<16;
    uint32_t pl=(w[9]&0xfff00000u)|0xfffffu;
    uint32_t btype=w[9]&15u, ltype=(w[9]>>16)&15u;
    if (btype!=ltype || (btype!=0u && btype!=1u)) return false;
    uint64_t base=pb, limit=pl;
    if (btype==1u) {
        base|=(uint64_t)w[10]<<32;
        limit|=(uint64_t)w[11]<<32;
    }
    if (base<=limit) {
        if (limit==UINT64_MAX) return false;
        out->prefetch_window=true;
        out->prefetch_base=base;
        out->prefetch_end=limit+1u;
    }
    return true;
}

bool pci796_queue_secondary(uint8_t secondary, uint8_t queue[256],
                            uint16_t *tail, bool visited[256])
{
    if (!queue || !tail || !visited || *tail>=256u || visited[secondary] ||
        !secondary) return false;
    visited[secondary]=true;
    queue[(*tail)++]=secondary;
    return true;
}
