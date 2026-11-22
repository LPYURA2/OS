#include <styleos/hardware_probe_793.h>
#include "efi.h"

#define PAGE UINT64_C(4096)
#define MAX_DESCRIPTORS 4096u
#define PHYS_MAX (UINT64_C(1)<<52)
#define RUNTIME_BIT (UINT64_C(1)<<63)

static int range(uint64_t base, uint64_t length, uint64_t *end)
{
    if (!length || base >= PHYS_MAX || length > PHYS_MAX-base) return 0;
    *end = base+length;
    return 1;
}

static const struct efi_memory_descriptor *entry(const struct boot_info *info,
                                                  uint32_t index)
{
    const uint8_t *map=(const uint8_t *)(uintptr_t)info->memory_map;
    return (const struct efi_memory_descriptor *)(const void *)(map+
                                (size_t)index*info->descriptor_size);
}

bool hw793_map_inspect(const struct boot_info *info, uint64_t base,
                       uint64_t bytes, struct hw793_map_report *out)
{
    uint64_t end;
    if (!out) return false;
    *out=(struct hw793_map_report){0};
    if (!info || info->magic!=STYLEOS_BOOT_MAGIC ||
        info->version!=STYLEOS_BOOT_VERSION ||
        info->size!=sizeof(*info) || info->arch!=STYLEOS_ARCH_X86_64 ||
        info->platform!=STYLEOS_PLATFORM_UEFI ||
        info->flags!=STYLEOS_BOOT_EXITED_SERVICES ||
        !info->memory_map || (info->memory_map & 7u) ||
        info->descriptor_version!=1u ||
        info->descriptor_size<sizeof(struct efi_memory_descriptor) ||
        info->descriptor_size>4096u || !info->memory_map_size ||
        info->memory_map_size%info->descriptor_size ||
        info->memory_map_size/info->descriptor_size>MAX_DESCRIPTORS ||
        info->memory_map>UINT64_MAX-info->memory_map_size ||
        !range(base,bytes,&end) || end>PHYS_MAX-PAGE) return false;
    out->page_start=base & ~(PAGE-1);
    out->page_end=(end+PAGE-1)&~(PAGE-1);
    out->descriptor_count=(uint32_t)(info->memory_map_size/info->descriptor_size);
    uint64_t next=out->page_start;
    /* Disallow malformed, overlapping descriptors. Firmware's descriptors are
     * data; never silently infer complete coverage from their mere presence. */
    for(uint32_t i=0;i<out->descriptor_count;i++){
        const struct efi_memory_descriptor *d=entry(info,i);
        uint64_t high;
        if ((d->physical_start&(PAGE-1)) ||
            d->number_of_pages>UINT64_MAX/PAGE ||
            (d->number_of_pages &&
             !range(d->physical_start,d->number_of_pages*PAGE,&high)))
            return false;
        if(d->type==5u || d->type==6u || (d->attribute&RUNTIME_BIT))
            out->runtime_count++;
        if(!d->number_of_pages) continue;
        for(uint32_t j=0;j<i;j++){
            const struct efi_memory_descriptor *prev=entry(info,j);
            if(prev->number_of_pages &&
               d->physical_start < prev->physical_start+prev->number_of_pages*PAGE &&
               prev->physical_start < high) return false;
        }
        if(d->physical_start<out->page_end && high>out->page_start){
            out->overlaps++;
            out->attribute_or|=d->attribute;
            if(out->overlaps==1)out->matched_type=d->type;
            else if(out->matched_type!=d->type)out->type_mixed=true;
        }
    }
    /* Advance across consecutive firmware descriptors, regardless of ordering. */
    for(uint32_t pass=0;pass<out->descriptor_count && next<out->page_end;pass++){
        uint64_t furthest=next;
        for(uint32_t i=0;i<out->descriptor_count;i++){
            const struct efi_memory_descriptor *d=entry(info,i);
            uint64_t top=d->physical_start+d->number_of_pages*PAGE;
            if(d->number_of_pages && d->physical_start<=next && top>furthest)
                furthest=top;
        }
        if(furthest==next)break;
        next=furthest;
    }
    if(next>out->page_end)next=out->page_end;
    out->covered_bytes=next-out->page_start;
    out->all_pages_described=next==out->page_end;
    return true;
}
