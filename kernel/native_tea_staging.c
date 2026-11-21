#include <styleos/native_tea_staging.h>

/* Keep all address-range arithmetic defined, including malformed pointers.
 * The trusted caller must also attest physical ownership, mapping and life. */
static bool span(uintptr_t first,size_t bytes,uintptr_t *last)
{
    if(!first || bytes>UINTPTR_MAX-first)return false;
    *last=first+bytes;return true;
}
static bool distinct(uintptr_t a,size_t an,uintptr_t b,size_t bn)
{
    uintptr_t ae,be;
    return span(a,an,&ae) && span(b,bn,&be) && (ae<=b || be<=a);
}
static void copy(uint8_t *dst,const uint8_t *src,size_t bytes)
{for(size_t i=0;i<bytes;++i)dst[i]=src[i];}

enum nts_status nts_prepare(const struct nts_device *disk,
                            const struct nts_pin *pin,
                            const struct nts_buffers *buffers,
                            struct nts_result *out)
{
    if(!disk || !pin || !buffers || !out || !disk->read ||
       !pin->package_sha || !pin->elf_sha || !pin->manifest ||
       !pin->manifest_bytes || !pin->package_bytes ||
       !buffers->sector || !buffers->package || !buffers->code)
        return NTS_ARGUMENT;
    if(((uintptr_t)buffers->code % NTE_PAGE) ||
       !distinct((uintptr_t)buffers->sector,NTP_SECTOR_BYTES,
                 (uintptr_t)buffers->package,NTP_MAX_PACKAGE) ||
       !distinct((uintptr_t)buffers->sector,NTP_SECTOR_BYTES,
                 (uintptr_t)buffers->code,NTE_PAGE) ||
       !distinct((uintptr_t)buffers->package,NTP_MAX_PACKAGE,
                 (uintptr_t)buffers->code,NTE_PAGE))return NTS_LAYOUT;
    if(pin->package_bytes<22u || pin->package_bytes>NTP_MAX_PACKAGE ||
       disk->sectors<2u)return NTS_ARGUMENT;
    const uint32_t blocks=(pin->package_bytes+NTP_SECTOR_BYTES-1u)/NTP_SECTOR_BYTES;
    if(blocks>disk->sectors-1u)return NTS_IO;
    if(!disk->read(disk->context,0,buffers->sector))return NTS_IO;
    uint32_t verified_bytes=0;
    if(ntp_disk_header(buffers->sector,pin->package_sha,&verified_bytes)!=NTP_OK ||
       verified_bytes!=pin->package_bytes)return NTS_PACKAGE;
    for(uint32_t i=0;i<blocks;++i){
        if(!disk->read(disk->context,i+1u,buffers->sector))return NTS_IO;
        const size_t offset=(size_t)i*NTP_SECTOR_BYTES;
        const size_t bytes=pin->package_bytes-offset<NTP_SECTOR_BYTES?
                           pin->package_bytes-offset:NTP_SECTOR_BYTES;
        copy(buffers->package+offset,buffers->sector,bytes);
    }
    struct ntp_payload payload={0};
    if(ntp_open(buffers->package,pin->package_bytes,pin->package_sha,
                pin->elf_sha,pin->manifest,pin->manifest_bytes,
                &payload)!=NTP_OK)return NTS_PACKAGE;
    struct nte_loaded loaded={0};
    if(nte_load_x64_page(payload.elf,payload.elf_bytes,pin->elf_sha,
                         buffers->code,NTE_PAGE,&loaded)!=NTE_OK)return NTS_ELF;
    *out=(struct nts_result){loaded,pin->package_bytes,disk->sectors};
    return NTS_OK;
}
