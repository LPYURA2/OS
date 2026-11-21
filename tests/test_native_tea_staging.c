#include <styleos/native_tea_staging.h>
#include "tea_769_expected.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISK_SECTORS 128u
static uint8_t disk_image[DISK_SECTORS*NTP_SECTOR_BYTES];
static _Alignas(NTE_PAGE) uint8_t code[NTE_PAGE];
static uint8_t sector[NTP_SECTOR_BYTES];
static uint8_t package[NTP_MAX_PACKAGE];
static unsigned read_count;
static uint32_t fail_at=UINT32_MAX;
static bool read_sector(void *context,uint32_t lba,uint8_t out[NTP_SECTOR_BYTES])
{
    const struct nts_device *device=context;
    ++read_count;
    if(!device || lba==fail_at || lba>=device->sectors)return false;
    memcpy(out,disk_image+(size_t)lba*NTP_SECTOR_BYTES,NTP_SECTOR_BYTES);
    return true;
}
static void load_image(const char *path)
{
    FILE *f=fopen(path,"rb");assert(f);
    assert(fread(disk_image,1,sizeof disk_image,f)==sizeof disk_image);
    assert(fgetc(f)==EOF);assert(!fclose(f));
}
static struct nts_pin pin(void)
{
    return (struct nts_pin){tea_769_pkg_sha,tea_769_elf_sha,
                            tea_769_expected_manifest,TEA_769_MANIFEST_SIZE,
                            TEA_769_PACKAGE_SIZE};
}
static struct nts_buffers buffers(void)
{return (struct nts_buffers){sector,package,code};}
static struct nts_device device(void)
{
    struct nts_device d={read_sector,NULL,DISK_SECTORS};
    return d;
}
static struct nts_device trusted;
static void checked(enum nts_status wanted,struct nts_pin p,struct nts_buffers b)
{
    struct nts_result result;
    memset(&result,0xa5,sizeof result);
    memset(code,0x7e,sizeof code);
    enum nts_status rc=nts_prepare(&trusted,&p,&b,&result);
    if(rc!=wanted){fprintf(stderr,"wanted %d, got %d\n",wanted,rc);abort();}
    if(rc!=NTS_OK){
        for(size_t i=0;i<sizeof code;++i)assert(code[i]==0x7e);
        const unsigned char *raw=(const unsigned char *)&result;
        for(size_t i=0;i<sizeof result;++i)assert(raw[i]==0xa5);
    }else{
        assert(result.loaded.entry==NTE_USER_CODE_VA);
        assert(result.loaded.code_bytes>0 && result.loaded.code_bytes<=NTE_PAGE);
        assert(result.package_bytes==TEA_769_PACKAGE_SIZE);
        assert(result.disk_sectors==DISK_SECTORS);
        assert(code[0]==0x90);
        assert(code[1]!=0x7e || code[2]!=0x7e);
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);
    load_image(argv[1]);
    trusted=device();trusted.context=&trusted;
    struct nts_pin p=pin();struct nts_buffers b=buffers();
    checked(NTS_OK,p,b);
    /* No out-of-range or overlapping slices can reach the disk callback. */
    b.package=code;read_count=0;
    checked(NTS_LAYOUT,p,b);assert(read_count==0);b=buffers();
    b.sector=package;read_count=0;
    checked(NTS_LAYOUT,p,b);assert(read_count==0);b=buffers();
    b.code=code+1;read_count=0;
    checked(NTS_LAYOUT,p,b);assert(read_count==0);b=buffers();
    p.package_bytes=NTP_MAX_PACKAGE+1u;read_count=0;
    checked(NTS_ARGUMENT,p,b);assert(read_count==0);p=pin();
    trusted.sectors=1;read_count=0;
    checked(NTS_ARGUMENT,p,b);assert(read_count==0);
    trusted.sectors=2;read_count=0;
    checked(NTS_IO,p,b);assert(read_count==0);
    trusted=device();trusted.context=&trusted;
    fail_at=0;checked(NTS_IO,p,b);fail_at=UINT32_MAX;
    fail_at=2;checked(NTS_IO,p,b);fail_at=UINT32_MAX;
    disk_image[0]^=1;checked(NTS_PACKAGE,p,b);disk_image[0]^=1;
    disk_image[NTP_SECTOR_BYTES+TEA_769_PACKAGE_SIZE/2]^=1;
    checked(NTS_PACKAGE,p,b);
    disk_image[NTP_SECTOR_BYTES+TEA_769_PACKAGE_SIZE/2]^=1;
    uint8_t fake_sha[32];memcpy(fake_sha,p.package_sha,32);fake_sha[0]^=1;
    p.package_sha=fake_sha;checked(NTS_PACKAGE,p,b);p=pin();
    uint8_t fake_elf_sha[32];memcpy(fake_elf_sha,p.elf_sha,32);fake_elf_sha[0]^=1;
    p.elf_sha=fake_elf_sha;checked(NTS_PACKAGE,p,b);p=pin();
    p.manifest_bytes=TEA_769_MANIFEST_SIZE-1u;
    checked(NTS_PACKAGE,p,b);p=pin();
    /* Known good disk remains executable after all failed preparations. */
    checked(NTS_OK,p,b);
    puts("STYLEOS 776 SHARED DISK TEA STAGING HOST PASS: bounded reads, hashes, ELF, IO refusal, no code writes on errors");
    return 0;
}
