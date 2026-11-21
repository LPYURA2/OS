#include <styleos/native_tea_elf.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tea_768_payload.h"

static _Alignas(4096) uint8_t page[NTE_PAGE];
static uint8_t data[TEA_768_ELF_SIZE];
static struct nte_loaded loaded;

static void copy_blob(void){memcpy(data,tea_768_elf,sizeof data);}
static void unchanged(enum nte_status expected)
{
    memset(page,0x66,sizeof page);
    loaded=(struct nte_loaded){55,66,77};
    assert(nte_load_x64_page(data,sizeof data,tea_768_sha256,page,sizeof page,&loaded)==expected);
    for(size_t i=0;i<sizeof page;++i)assert(page[i]==0x66);
    assert(loaded.entry==55 && loaded.code_bytes==66 && loaded.memory_bytes==77);
}
int main(void)
{
    copy_blob();
    assert(nte_load_x64_page(data,sizeof data,tea_768_sha256,page,sizeof page,&loaded)==NTE_OK);
    assert(loaded.entry==NTE_USER_CODE_VA);
    assert(loaded.code_bytes>TEA_768_EXIT_RESUME_OFFSET);
    assert(page[0]==0x90 && page[TEA_768_YIELD_OFFSET]==0xcd &&
           page[TEA_768_EXIT_OFFSET]==0xcd);
    for(size_t i=loaded.code_bytes;i<sizeof page;++i)assert(page[i]==0);
    copy_blob();data[0]=0;unchanged(NTE_FORMAT);
    copy_blob();data[18]=0;unchanged(NTE_FORMAT);
    copy_blob();data[24]++;unchanged(NTE_RIGHTS);
    copy_blob();data[64+4]=7;unchanged(NTE_RIGHTS); /* RWX */
    copy_blob();data[64+32]=0xff;unchanged(NTE_RIGHTS); /* filesz > memsz */
    copy_blob();data[64+8]=0xff;unchanged(NTE_BOUNDS); /* not page aligned */
    copy_blob();data[4096]^=1;unchanged(NTE_HASH);
    copy_blob();
    uint8_t bad_hash[32];memcpy(bad_hash,tea_768_sha256,sizeof bad_hash);bad_hash[0]^=1;
    memset(page,0x66,sizeof page);
    assert(nte_load_x64_page(data,sizeof data,bad_hash,page,sizeof page,&loaded)==NTE_HASH);
    puts("NATIVE TEA ELF HOST PASS: trusted one-page ELF, W^X, hash, bounds, no partial writes");
    return 0;
}
