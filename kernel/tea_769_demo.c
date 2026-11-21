#include <styleos/arch.h>
#include <styleos/tea_768_demo.h>
#include <styleos/native_tea_staging.h>
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

static bool disk_read(void *context,uint32_t lba,uint8_t output[NTP_SECTOR_BYTES])
{
    (void)context;
    return tea_769_disk_sector(lba,output)!=0;
}
int tea_768_prepare(void)
{
    uint32_t sectors=0;
    if(!tea_769_disk_init(&sectors))return 0;
    const struct nts_device disk={disk_read,0,sectors};
    const struct nts_pin pin={tea_769_pkg_sha,tea_769_elf_sha,
                              tea_769_expected_manifest,TEA_769_MANIFEST_SIZE,
                              TEA_769_PACKAGE_SIZE};
    const struct nts_buffers buffers={styleos_tea_769_sector,
                                      styleos_tea_769_package,
                                      styleos_tea_768_code_b};
    struct nts_result prepared={0};
    if(nts_prepare(&disk,&pin,&buffers,&prepared)!=NTS_OK)return 0;
    loaded=prepared.loaded;
    if(loaded.entry!=NTE_USER_CODE_VA || loaded.code_bytes<=TEA_768_EXIT_OFFSET+1u ||
       TEA_768_YIELD_OFFSET>=TEA_768_RESUME_OFFSET ||
       TEA_768_RESUME_OFFSET>=TEA_768_EXIT_OFFSET ||
       TEA_768_EXIT_OFFSET>=TEA_768_EXIT_RESUME_OFFSET ||
       styleos_tea_768_code_b[0]!=0x90u ||
       styleos_tea_768_code_b[TEA_768_YIELD_OFFSET]!=0xcdu ||
       styleos_tea_768_code_b[TEA_768_YIELD_OFFSET+1]!=0x80u ||
       styleos_tea_768_code_b[TEA_768_EXIT_OFFSET]!=0xcdu ||
       styleos_tea_768_code_b[TEA_768_EXIT_OFFSET+1]!=0x80u)return 0;
    arch_puts("STYLEOS TEA776 SHARED STAGING PASS\n");
    arch_puts("STYLEOS TEA769 DISK LOAD PASS bytes=");decimal(prepared.package_bytes);
    arch_puts(" sectors=");decimal(prepared.disk_sectors);arch_putc('\n');
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
