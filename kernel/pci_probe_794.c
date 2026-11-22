#include <styleos/pci_probe_794.h>
#include <stddef.h>

bool pci794_parse_header(const uint32_t config[16], struct pci794_device *out)
{
    if (!out) return false;
    *out=(struct pci794_device){0};
    if (!config || (config[0]&0xffffu)==0xffffu ||
        (config[0]&0xffffu)==0 ||
        ((config[3]>>16)&0x7fu)!=0) return false;
    out->vendor=(uint16_t)config[0];
    out->device=(uint16_t)(config[0]>>16);
    out->command=(uint16_t)config[1];
    out->class_code=(uint8_t)(config[2]>>24);
    out->subclass=(uint8_t)(config[2]>>16);
    out->header_type=(uint8_t)(config[3]>>16);
    for (uint8_t i=0;i<6;i++) {
        uint32_t raw=config[4+i];
        if (!raw || raw==UINT32_MAX) continue;
        struct pci794_bar entry={.index=i,.raw_lo=raw};
        if (raw&1u) {
            entry.memory=false;
            entry.width=32;
            entry.base=(uint64_t)(raw&~3u);
        } else {
            uint32_t type=(raw>>1)&3u;
            if (type==1u || type==3u) return false; /* reserved PCI BAR type */
            entry.memory=true;
            entry.prefetch=!!(raw&8u);
            entry.width=type==2u?64:32;
            entry.base=(uint64_t)(raw&~15u);
            if (type==2u) {
                if (i==5) return false;
                entry.raw_hi=config[5+i];
                entry.base|=(uint64_t)entry.raw_hi<<32;
                ++i;
            }
        }
        if (!entry.base) continue; /* unassigned BAR; do not invent a range */
        out->bars[out->bar_count++]=entry;
    }
    return true;
}

bool pci794_find_framebuffer_bar(const struct pci794_device *device,
                                 uint64_t framebuffer_base,
                                 struct pci794_bar *out)
{
    if (!device || !out || !framebuffer_base || framebuffer_base>=(UINT64_C(1)<<52) ||
        device->class_code!=0x03u ||
        (device->subclass!=0u && device->subclass!=0x80u)) return false;
    for (uint8_t i=0;i<device->bar_count;i++) {
        struct pci794_bar bar=device->bars[i];
        if (bar.memory && bar.base==framebuffer_base && bar.base<(UINT64_C(1)<<52)) {
            *out=bar;
            return true;
        }
    }
    return false;
}
