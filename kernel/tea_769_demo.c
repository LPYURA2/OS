#include <styleos/arch.h>
#include <styleos/tea_768_demo.h>
#include <styleos/native_tea_package.h>
#include <styleos/tea_769_disk_io.h>
#include "tea_769_expected.h"

/* Dedicated x86 CPL3 test only. Both buffers belong to verified image/BSS;
 * no allocator, no filesystem mount and no direct user-mode disk I/O. */
_Alignas(4096) unsigned char styleos_tea_768_code_b[NTE_PAGE];
_Alignas(4096) unsigned char styleos_tea_769_package[NTP_MAX_PACKAGE];
_Alignas(4096) unsigned char styleos_tea_769_sector[NTE_PAGE];
static struct nte_loaded loaded;

static void decimal(uint32_t x)
{
    char digits[10];unsigned n=0;
    do{digits[n++]=(char)('0'+x%10);x/=10;}while(x && n<sizeof digits);
    while(n)arch_putc(digits[--n]);
}

int tea_768_prepare(void)
{
    uint32_t sectors=0,bytes=0;
    if(!tea_769_disk_init(&sectors))return 0;
    if(!tea_769_disk_sector(0,styleos_tea_769_sector))return 0;
    if(ntp_disk_header(styleos_tea_769_sector,tea_769_pkg_sha,&bytes)!=NTP_OK ||
       bytes!=TEA_769_PACKAGE_SIZE || 1u+(bytes+511u)/512u>sectors)return 0;
    for(uint32_t k=0;k<(bytes+511u)/512u;++k){
        if(!tea_769_disk_sector(1u+k,styleos_tea_769_sector))return 0;
        size_t at=(size_t)k*512u;
        size_t copy=bytes-at<512u?bytes-at:512u;
        for(size_t j=0;j<copy;++j)styleos_tea_769_package[at+j]=styleos_tea_769_sector[j];
    }
    struct ntp_payload payload={0};
    if(ntp_open(styleos_tea_769_package,bytes,tea_769_pkg_sha,tea_769_elf_sha,
                tea_769_expected_manifest,TEA_769_MANIFEST_SIZE,&payload)!=NTP_OK)return 0;
    if(nte_load_x64_page(payload.elf,payload.elf_bytes,tea_769_elf_sha,
                         styleos_tea_768_code_b,sizeof(styleos_tea_768_code_b),
                         &loaded)!=NTE_OK)return 0;
    if(loaded.entry!=NTE_USER_CODE_VA || loaded.code_bytes<=TEA_768_EXIT_OFFSET+1u ||
       TEA_768_YIELD_OFFSET>=TEA_768_RESUME_OFFSET ||
       TEA_768_RESUME_OFFSET>=TEA_768_EXIT_OFFSET ||
       TEA_768_EXIT_OFFSET>=TEA_768_EXIT_RESUME_OFFSET ||
       styleos_tea_768_code_b[0]!=0x90u ||
       styleos_tea_768_code_b[TEA_768_YIELD_OFFSET]!=0xcdu ||
       styleos_tea_768_code_b[TEA_768_YIELD_OFFSET+1]!=0x80u ||
       styleos_tea_768_code_b[TEA_768_EXIT_OFFSET]!=0xcdu ||
       styleos_tea_768_code_b[TEA_768_EXIT_OFFSET+1]!=0x80u)return 0;
    arch_puts("STYLEOS TEA769 DISK LOAD PASS bytes=");decimal(bytes);
    arch_puts(" sectors=");decimal(sectors);arch_putc('\n');
    return 1;
}
uint64_t tea_768_yield_pc(void){return NTE_USER_CODE_VA+TEA_768_YIELD_OFFSET;}
uint64_t tea_768_resume_pc(void){return NTE_USER_CODE_VA+TEA_768_RESUME_OFFSET;}
uint64_t tea_768_exit_pc(void){return NTE_USER_CODE_VA+TEA_768_EXIT_OFFSET;}
void tea_768_report(uint32_t pid)
{
    arch_puts("STYLEOS TEA769 PKG sha256=" TEA_769_PKG_SHA "\n");
    arch_puts("STYLEOS TEA769 ELF sha256=" TEA_769_ELF_SHA "\n");
    arch_puts("STYLEOS TEA769 ADMITTED pid=");decimal(pid);
    arch_puts(" entry=0x10000000\n");
}
