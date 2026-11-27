#include <styleos/native_tea_package.h>
#include <styleos/native_tea_elf.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint8_t package[NTP_MAX_PACKAGE+64], disk[512], altered[NTP_MAX_PACKAGE+64];
static uint8_t sha_pkg[32],sha_elf[32],manifest[4096],scratch[4096];
static _Alignas(4096) uint8_t page[4096];
static uint8_t reader_scratch[8192],reader_sector[512];
static uint32_t read32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void write32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static size_t contents(const char *name,uint8_t *p,size_t max)
{FILE *f=fopen(name,"rb");assert(f);size_t n=fread(p,1,max,f);assert(!ferror(f));assert(fgetc(f)==EOF);fclose(f);return n;}
static enum ntp_status open_modified(size_t n)
{
    uint8_t new_hash[32];nte_sha256(altered,n,new_hash);
    struct ntp_payload v={0};return ntp_open(altered,n,new_hash,sha_elf,manifest,
                                                read32(disk+48),&v);
}
int main(int argc,char **argv)
{
    assert(argc==4);
    size_t count=contents(argv[1],package,sizeof package);
    size_t length=contents(argv[2],disk,sizeof disk);
    size_t msize=contents(argv[3],manifest,sizeof manifest);
    assert(length==sizeof disk && count>22 && count<NTP_MAX_PACKAGE);
    memcpy(sha_pkg,disk+16,32);
    /* Host fixture uses a separately pinned expected manifest. */
    write32(disk+48,1);
    uint32_t bytes=0;
    assert(ntp_disk_header(disk,sha_pkg,&bytes)==NTP_FORMAT); /* reserved nonzero */
    write32(disk+48,0);
    assert(ntp_disk_header(disk,sha_pkg,&bytes)==NTP_OK && bytes==count);
    /* Discover expected verified ELF digest from the canonical central ZIP
     * but independently validate it through the narrow ELF loader and fixture. */
    size_t at=30+(size_t)package[26]+((size_t)package[27]<<8);
    at+=msize;
    assert(at+30<count && read32(package+at)==0x04034b50u);
    size_t elf_at=at+30+package[at+26]+((size_t)package[at+27]<<8);
    size_t elf_bytes=read32(package+at+18);
    assert(elf_at+elf_bytes<=count);
    nte_sha256(package+elf_at,elf_bytes,sha_elf);
    struct ntp_payload view={0};
    assert(ntp_open(package,count,sha_pkg,sha_elf,manifest,msize,&view)==NTP_OK);
    assert(view.elf==package+elf_at && view.elf_bytes==elf_bytes);
    struct nte_loaded loaded={0};
    assert(nte_load_x64_page(view.elf,view.elf_bytes,sha_elf,page,sizeof page,&loaded)==NTE_OK);
    assert(loaded.entry==NTE_USER_CODE_VA);
    /* The actual runtime sector-reader adapter must return exact ELF bytes,
     * zero pad the final block, refuse out-of-range and aliased destinations,
     * and work with the existing authenticated loader. */
    assert(view.elf_bytes<=8192);
    uint32_t blocks=(uint32_t)((view.elf_bytes+511)/512);
    for(uint32_t block=0;block<blocks;++block){
        assert(ntp_elf_sector(&view,block,reader_sector));
        for(unsigned j=0;j<512;++j){
            size_t at=(size_t)block*512+j;
            assert(reader_sector[j]==(at<view.elf_bytes?view.elf[at]:0));
        }
    }
    memset(reader_sector,0x5a,sizeof reader_sector);
    assert(!ntp_elf_sector(&view,blocks,reader_sector));
    assert(!ntp_elf_sector(&view,UINT32_MAX,reader_sector));
    for(unsigned j=0;j<512;++j)assert(reader_sector[j]==0x5a);
    assert(!ntp_elf_sector(&view,0,(uint8_t *)view.elf));
    assert(!ntp_elf_sector(&view,0,(uint8_t *)&view));
    struct nte_loaded via={0};
    memset(page,0x77,sizeof page);
    assert(nte_load_x64_reader(ntp_elf_sector,&view,0,blocks,view.elf_bytes,
        sha_elf,reader_scratch,sizeof reader_scratch,page,sizeof page,&via)==NTE_OK);
    assert(via.entry==loaded.entry&&via.code_bytes==loaded.code_bytes&&via.memory_bytes==loaded.memory_bytes);
    memset(page,0x77,sizeof page);via=(struct nte_loaded){123,456,789};
    uint8_t wrong[32];memcpy(wrong,sha_elf,32);wrong[0]^=1;
    assert(nte_load_x64_reader(ntp_elf_sector,&view,0,blocks,view.elf_bytes,
        wrong,reader_scratch,sizeof reader_scratch,page,sizeof page,&via)==NTE_HASH);
    assert(via.entry==123&&via.code_bytes==456&&via.memory_bytes==789);
    for(unsigned j=0;j<sizeof page;++j)assert(page[j]==0x77);
    /* Zeroize on disk/header refusal, and prove no output exposure. */
    memset(scratch,0x69,sizeof scratch);view=(struct ntp_payload){scratch,77};
    assert(ntp_open(package,count,sha_pkg,sha_elf,manifest,msize-1,&view)==NTP_FORMAT);
    assert(view.elf==scratch && view.elf_bytes==77);
    memcpy(altered,package,count);altered[count/2]^=1;
    assert(ntp_open(altered,count,sha_pkg,sha_elf,manifest,msize,&view)==NTP_HASH);
    assert(view.elf==scratch && view.elf_bytes==77);
    assert(ntp_open(package,count-1,sha_pkg,sha_elf,manifest,msize,&view)==NTP_HASH);
    assert(ntp_open(package,NTP_MAX_PACKAGE+1,sha_pkg,sha_elf,manifest,msize,&view)==NTP_BOUNDS);
    /* Even when the expected whole-package digest is updated to the tampered
     * bytes, the ZIP directory, names, trailer and CRC still reject. */
    memcpy(altered,package,count);altered[count-22]=0;
    assert(open_modified(count)!=NTP_OK);
    memcpy(altered,package,count);altered[0]=0;
    assert(open_modified(count)!=NTP_OK);
    memcpy(altered,package,count);altered[30]='X';
    assert(open_modified(count)!=NTP_OK);
    memcpy(altered,package,count);altered[at+14]^=1;
    assert(open_modified(count)!=NTP_OK);
    memcpy(altered,package,count);altered[count-6]=0xff;
    assert(open_modified(count)!=NTP_OK);
    memcpy(altered,package,count);altered[count-20]=1;
    assert(open_modified(count)!=NTP_OK);
    memcpy(altered,package,count);altered[elf_at]^=1;
    assert(open_modified(count)!=NTP_OK);
    memcpy(disk,scratch,512); /* untrusted/random header rejected */
    assert(ntp_disk_header(disk,sha_pkg,&bytes)!=NTP_OK);
    assert(ntp_disk_header(NULL,sha_pkg,&bytes)==NTP_ARGUMENT);
    assert(ntp_open(NULL,count,sha_pkg,sha_elf,manifest,msize,&view)==NTP_ARGUMENT);
    puts("TEA769 HOST PASS: canonical disk ZIP + manifest + CRC + hash + ELF + refusals");
    return 0;
}
