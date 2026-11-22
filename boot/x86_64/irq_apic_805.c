#include <styleos/irq_apic_805.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
/* Terminal QEMU/OVMF only. Post-ExitBootServices CPU snapshot. NO PIC masks,
 * LAPIC MSR, IOAPIC registers, IDT gates or interrupt controls are changed. */
static void hx(uint64_t v){arch_puts("0x");for(int n=60;n>=0;n-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)n)&15u]);}
static void fld(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');hx(v);}
static uint64_t rdmsr(uint32_t n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return ((uint64_t)d<<32)|a;}
static void cp1(uint32_t *c,uint32_t *d){uint32_t a,b;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(*c),"=d"(*d):"a"(1),"c"(0));}
static uint8_t in8(uint16_t port){uint8_t x;__asm__ volatile("inb %1,%0":"=a"(x):"Nd"(port));return x;}
struct idtptr805 {uint16_t limit;uint64_t base;} __attribute__((packed));
static _Noreturn void fail(const char *reason){arch_puts("STYLEOS S805 FAIL ");arch_puts(reason);arch_putc('\n');arch_halt();}
_Noreturn void irq805_terminal(const struct boot_info *info){
    if(!info||info->magic!=STYLEOS_BOOT_MAGIC||info->flags!=STYLEOS_BOOT_EXITED_SERVICES)fail("handoff");
    struct irq805_sample s={0};struct idtptr805 idtr={0};
    cp1(&s.cpuid_ecx,&s.cpuid_edx);
    if(!(s.cpuid_edx&(UINT32_C(1)<<5)))fail("msr-unsupported");
    s.apic_base=rdmsr(0x1bu);
    __asm__ volatile("sidt %0":"=m"(idtr));
    __asm__ volatile("pushfq; popq %0":"=r"(s.rflags));
    __asm__ volatile("mov %%cr3,%0":"=r"(s.cr3));
    s.idtr_base=idtr.base;s.idtr_limit=idtr.limit;
    s.pic_master=in8(0x21u);s.pic_slave=in8(0xa1u);
    uint64_t physical=0;
    if(!irq805_validate(&s,&physical))fail("invalid-snapshot");
    arch_puts("STYLEOS S805 CPU");fld("cpuid-ecx",s.cpuid_ecx);fld("cpuid-edx",s.cpuid_edx);
    fld("apic-base-msr",s.apic_base);fld("lapic-physical",physical);
    fld("apic-global",(s.apic_base>>11)&1u);fld("x2-mode",(s.apic_base>>10)&1u);
    fld("bsp",(s.apic_base>>8)&1u);arch_putc('\n');
    arch_puts("STYLEOS S805 INTERRUPTS");fld("idtr-base",s.idtr_base);
    fld("idtr-limit",s.idtr_limit);fld("rflags",s.rflags);fld("if-enabled",(s.rflags>>9)&1u);
    fld("pic-master",s.pic_master);fld("pic-slave",s.pic_slave);fld("cr3",s.cr3);arch_putc('\n');
    arch_puts("STYLEOS S805 POLICY cpu-apic-snapshot=yes idtr-observed=yes pic-mask-observed=yes madt-unverified=yes ioapic-routing-unverified=yes irq-delivery-unverified=yes dma-iommu-unverified=yes firmware-runtime-unverified=yes production-cr3=deny\n");
    arch_puts("STYLEOS S805 PASS\n");
    __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));
    arch_halt();
}
