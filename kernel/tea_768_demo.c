#include <styleos/tea_768_demo.h>
#include <styleos/arch.h>
#include "tea_768_payload.h"

/* Image-owned storage; never a general user-space physical allocator. The
 * inherited UEFI page mapping is proven writable before the first write. */
_Alignas(4096) unsigned char styleos_tea_768_code_b[NTE_PAGE];
static struct nte_loaded loaded;

int tea_768_prepare(void)
{
    if(nte_load_x64_page(tea_768_elf,TEA_768_ELF_SIZE,tea_768_sha256,
                         styleos_tea_768_code_b,sizeof(styleos_tea_768_code_b),
                         &loaded)!=NTE_OK) return 0;
    return loaded.entry==NTE_USER_CODE_VA && loaded.code_bytes>TEA_768_EXIT_OFFSET+1u &&
           TEA_768_YIELD_OFFSET<TEA_768_RESUME_OFFSET &&
           TEA_768_RESUME_OFFSET<TEA_768_EXIT_OFFSET &&
           TEA_768_EXIT_OFFSET<TEA_768_EXIT_RESUME_OFFSET &&
           styleos_tea_768_code_b[0]==0x90u &&
           styleos_tea_768_code_b[TEA_768_YIELD_OFFSET]==0xcdu &&
           styleos_tea_768_code_b[TEA_768_YIELD_OFFSET+1u]==0x80u &&
           styleos_tea_768_code_b[TEA_768_EXIT_OFFSET]==0xcdu &&
           styleos_tea_768_code_b[TEA_768_EXIT_OFFSET+1u]==0x80u;
}
uint64_t tea_768_yield_pc(void){return NTE_USER_CODE_VA+TEA_768_YIELD_OFFSET;}
uint64_t tea_768_resume_pc(void){return NTE_USER_CODE_VA+TEA_768_RESUME_OFFSET;}
uint64_t tea_768_exit_pc(void){return NTE_USER_CODE_VA+TEA_768_EXIT_OFFSET;}
void tea_768_report(uint32_t pid)
{
    arch_puts("STYLEOS TEA768 PKG sha256=" TEA_768_PKG_SHA "\n");
    arch_puts("STYLEOS TEA768 ELF sha256=" TEA_768_ELF_SHA "\n");
    arch_puts("STYLEOS TEA768 ADMITTED pid=");
    /* Decimal PID for independent review; no fake UI session IDs. */
    char digits[10];unsigned count=0;
    do{digits[count++]=(char)('0'+pid%10u);pid/=10u;}while(pid && count<10);
    while(count)arch_putc(digits[--count]);
    arch_puts(" entry=0x10000000\n");
}
