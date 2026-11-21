#include <styleos/kernel_dependency_inventory.h>

static uint16_t le16(const unsigned char *p)
{ return (uint16_t)((unsigned)p[0] | (unsigned)p[1]<<8); }
static uint32_t le32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static bool span(size_t size,size_t pos,size_t count)
{ return pos<=size && count<=size-pos; }

bool kdi_profile_loaded_pe(const void *loaded, size_t loaded_size,
                           struct kdi_pe_profile *profile)
{
    if (profile) *profile=(struct kdi_pe_profile){0};
    if (!profile || !loaded || loaded_size<256 || loaded_size>64u*1024u*1024u ||
        (loaded_size&4095u)) return false;
    const unsigned char *p=(const unsigned char*)loaded;
    if (le16(p)!=0x5a4d) return false;
    size_t pe=le32(p+0x3c);
    if (!span(loaded_size,pe,24) || le32(p+pe)!=0x00004550u) return false;
    uint32_t count=le16(p+pe+6), opt=le16(p+pe+20);
    if (count<1 || count>32 || opt<64 || !span(loaded_size,pe+24,opt) ||
        le16(p+pe+24)!=0x020bu) return false;
    uint32_t image_size=le32(p+pe+24+56);
    uint32_t headers=le32(p+pe+24+60);
    if (image_size<loaded_size || image_size>64u*1024u*1024u ||
        headers==0 || headers>loaded_size ||
        !span(loaded_size,pe+24+opt,(size_t)count*40u) ||
        pe+24+opt+(size_t)count*40u > headers) return false;
    size_t sections=pe+24+opt;
    struct kdi_pe_profile tmp={0};
    tmp.sections=count;
    tmp.declared_extra_pages=(uint32_t)((image_size-loaded_size+4095u)/4096u);
    tmp.image_pages=(uint32_t)(loaded_size/4096);
    for (size_t page=0;page<loaded_size;page+=4096u) {
        unsigned role=page<headers ? 1u : 0u; /* PE headers are read only */
        for(uint32_t i=0;i<count;++i) {
            const unsigned char *sec=p+sections+(size_t)i*40u;
            uint32_t size=le32(sec+8), va=le32(sec+12);
            uint32_t flags=le32(sec+36);
            if (size==0) continue;
            if (!va || va>=image_size || size>image_size-va ||
                (!(flags&0x40000000u) && !(flags&0x80000000u) &&
                 !(flags&0x20000000u))) return false;
            uint64_t a=va,b=(uint64_t)va+size;
            if ((uint64_t)page<b && (uint64_t)page+4096u>a) {
                if (flags&0x20000000u) role|=2u;
                if (flags&0x80000000u) role|=4u;
                if (!(flags&0xa0000000u)) role|=1u;
            }
        }
        if (!role) ++tmp.uncovered_pages;
        else if ((role&6u)==6u) ++tmp.mixed_pages;
        else if (role&2u) ++tmp.text_pages;
        else if (role&4u) ++tmp.writable_pages;
        else ++tmp.readonly_pages;
    }
    *profile=tmp;
    return true;
}

bool kdi_audit_dependencies(const struct kdi_pe_profile *pe,
                            const struct kdi_live_dependencies *d,
                            struct kdi_report *out)
{
    if (out) *out=(struct kdi_report){.blockers=UINT32_MAX,
                                      .activation_permitted=false};
    if (!pe || !d || !out || !pe->sections || !pe->image_pages ||
        !pe->text_pages || !(pe->writable_pages||pe->mixed_pages) ||
        pe->text_pages+pe->writable_pages+pe->readonly_pages+
          pe->mixed_pages+pe->uncovered_pages!=pe->image_pages)
        return false;
    uint32_t missing=0;
    if (!d->tr_selector || !d->tss_ist_proved) missing|=KDI_TSS_IST;
    if (!d->all_stacks_proved) missing|=KDI_OTHER_STACKS;
    if (!d->framebuffer_proved) missing|=KDI_FRAMEBUFFER;
    if (!d->device_mmio_proved) missing|=KDI_DEVICE_MMIO;
    if (!d->firmware_runtime_proved) missing|=KDI_FIRMWARE_RUNTIME;
    if (!d->kernel_wx_proved || d->inherited_rwx_pages)
        missing|=KDI_WX_POLICY;
    if (!d->cache_policy_proved) missing|=KDI_CACHE_POLICY;
    if (!d->physical_ownership_proved) missing|=KDI_PHYSICAL_OWNERSHIP;
    if (pe->mixed_pages || pe->uncovered_pages || pe->declared_extra_pages)
        missing|=KDI_PE_CONFLICT;
    out->blockers=missing;
    out->activation_permitted=false;
    return true;
}
