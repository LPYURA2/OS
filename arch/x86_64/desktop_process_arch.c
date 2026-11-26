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

#if defined(STYLEOS_DESKTOP_V0) || (defined(STYLEOS_TEST_RUNTIME_ENTRY_858) || defined(STYLEOS_OWNED_SHELL_865))
#include <styleos/runtime_entry_858.h>
struct rt864_desc {uint16_t limit;uint64_t base;} __attribute__((packed));
/* One privileged observation path for boot, dispatch and shell safe points.
 * No firmware call, no port/MMIO access, no control-register modification. */
struct rt858_live rt864_arch_capture(void)
{
    struct rt858_live v={0};struct rt864_desc g,d;
    __asm__ volatile("sgdt %0;sidt %1;str %2;mov %%cs,%3;mov %%rsp,%4;mov %%cr0,%5;mov %%cr4,%6;mov %%cr3,%7;pushfq;popq %8":"=m"(g),"=m"(d),"=r"(v.tr),"=r"(v.cs),"=r"(v.rsp),"=r"(v.cr0),"=r"(v.cr4),"=r"(v.root),"=r"(v.rflags)::"memory");
    uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000080));
    v.efer=((uint64_t)hi<<32)|lo;v.gdt=g.base;v.idt=d.base;v.gdt_limit=g.limit;v.idt_limit=d.limit;
    return v;
}
bool rt864_arch_shell_boundary(void)
{
    struct rt858_live v=rt864_arch_capture();
    return rt864_shell_checkpoint(&v);
}
#endif
