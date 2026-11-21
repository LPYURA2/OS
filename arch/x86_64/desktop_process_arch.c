#include <styleos/desktop_process_runtime.h>

/* Read the CPU's real current page-table root, not a desktop session ID.
 * CR3 low bits may contain PCID or cache flags and are not part of the root. */
uint64_t dpr_arch_safe_root(void)
{
    uint64_t cr0 = 0, cr3 = 0, cr4 = 0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    /* Paging and PAE must already be active in x86-64 UEFI long mode. */
    if (!(cr0 & (UINT64_C(1) << 31)) ||
        !(cr4 & (UINT64_C(1) << 5))) return 0;
    return cr3 & ~UINT64_C(4095);
}
