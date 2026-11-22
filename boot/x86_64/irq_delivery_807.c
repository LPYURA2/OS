#include "efi.h"
#include <styleos/acpi_madt_806.h>
#include <styleos/irq_route_807.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
/* ISOLATED TERMINAL QEMU ONLY. No code is linked to ordinary Desktop.
 * PIT0 -> ISA IRQ0 -> MADT ISO GSI2 -> IOAPIC -> LAPIC vector 0x50.
 * All MMIO, timer and IDT changes occur after ExitBootServices. */
static volatile uint32_t irq807_hits,irq807_isr_seen,irq807_eoi_clear;
static volatile uint8_t *irq807_lapic_ptr;
__asm__(
 ".global irq807_handler\n"
 "irq807_handler:\n"
 "pushq %rax\n"
 "pushq %rdx\n"
 "incl irq807_hits(%rip)\n"
 "movq irq807_lapic_ptr(%rip),%rax\n"
 "movl 0x120(%rax),%edx\n" /* vector 0x50 ISR register, bit 16 */
 "testl $0x10000,%edx\n"
 "jz 1f\n"
 "orl $1,irq807_isr_seen(%rip)\n"
 "1: movl $0,0xb0(%rax)\n" /* LAPIC EOI */
 "movl 0x120(%rax),%edx\n"
 "testl $0x10000,%edx\n"
 "jnz 2f\n"
 "orl $1,irq807_eoi_clear(%rip)\n"
 "2: popq %rdx\n"
 "popq %rax\n"
 "iretq\n"
 ".global irq807_unexpected\n"
 "irq807_unexpected:\n"
 "cli\n"
 "movl $0x22,%eax\n"
 "outl %eax,$0xf4\n" /* unexpected interrupt -> failure exit 69 */
 "3: hlt\n"
 "jmp 3b\n"
);
void irq807_handler(void);void irq807_unexpected(void);
struct idt_gate807 {uint16_t offset_low,selector;uint8_t ist,attrs;uint16_t offset_mid;uint32_t offset_high,reserved;}__attribute__((packed));
struct idtr807 {uint16_t limit;uint64_t base;}__attribute__((packed));
static struct idt_gate807 idt[256] __attribute__((aligned(16)));
static void gate(uint32_t vector,void (*handler)(void),uint16_t cs){
 uintptr_t p=(uintptr_t)handler;
 idt[vector]=(struct idt_gate807){(uint16_t)p,cs,0,0x8e,(uint16_t)(p>>16),(uint32_t)(p>>32),0};
}
static uint64_t rdmsr(uint32_t msr){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(msr));return ((uint64_t)d<<32)|a;}
static uint64_t tsc(void){uint32_t a,d;__asm__ volatile("rdtsc":"=a"(a),"=d"(d));return ((uint64_t)d<<32)|a;}
static uint8_t inb(uint16_t p){uint8_t x;__asm__ volatile("inb %1,%0":"=a"(x):"Nd"(p));return x;}
static void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static uint32_t mmio_read(uintptr_t b,uint32_t o){return *(volatile uint32_t *)(b+o);}
static void mmio_write(uintptr_t b,uint32_t o,uint32_t v){*(volatile uint32_t *)(b+o)=v;}
static uint32_t io_read(uintptr_t io,uint32_t index){mmio_write(io,0,index);return mmio_read(io,0x10);}
static void io_write(uintptr_t io,uint32_t index,uint32_t value){mmio_write(io,0,index);mmio_write(io,0x10,value);}
static void hx(uint64_t v){arch_puts("0x");for(int k=60;k>=0;k-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)k)&15u]);}
static void fld(const char *key,uint64_t val){arch_putc(' ');arch_puts(key);arch_putc('=');hx(val);}
static _Noreturn void fail(const char *reason){__asm__ volatile("cli");arch_puts("STYLEOS S807 FAIL ");arch_puts(reason);arch_putc('\n');arch_halt();}
_Noreturn void irq807_terminal(const struct boot_info *b){
 if(!b||b->magic!=STYLEOS_BOOT_MAGIC||b->flags!=STYLEOS_BOOT_EXITED_SERVICES)fail("handoff");
 struct acpi806_topology t={0};if(!acpi806_copy_capture(&t))fail("acpi-topology");
 if(t.ioapic_count!=1||t.enabled_lapics!=1)fail("unsupported-multiprocessor-or-ioapic");
 uint32_t lapic_msrlow=(uint32_t)rdmsr(0x1b);
 if(!(lapic_msrlow&(1u<<11))||(lapic_msrlow&(1u<<10))||(lapic_msrlow&0xfffff000u)!=t.lapic_physical)fail("lapic-mode-or-base");
 uintptr_t lapic=t.lapic_physical, io=t.ioapic[0].physical;
 uint32_t io_id=(io_read(io,0)>>24)&15u,io_version=io_read(io,1),counts[ACPI806_MAX_IOAPIC]={0};
 counts[0]=((io_version>>16)&255u)+1u;
 if(io_id!=t.ioapic[0].id)fail("ioapic-id");
 struct irq807_plan plan={0};if(!irq807_select_pit(&t,counts,&plan)||plan.ioapic_index!=0)fail("madt-irq0-route");
 uint32_t dest=mmio_read(lapic,0x20)>>24;
 if(dest>255u)fail("lapic-id");
 uint32_t reg=0x10+2*plan.pin;
 uint32_t original_low=io_read(io,reg),original_high=io_read(io,reg+1);
 uint32_t original_tpr=mmio_read(lapic,0x80),original_svr=mmio_read(lapic,0xf0);
 uint64_t old_rflags=0;
 __asm__ volatile("pushfq;popq %0":"=r"(old_rflags));
 if((old_rflags&(1u<<9))!=0||inb(0x21)!=0xff||inb(0xa1)!=0xff)fail("interrupt-precondition");
 struct idtr807 old_idt={0},new_idt={sizeof(idt)-1u,(uint64_t)(uintptr_t)&idt[0]};
 __asm__ volatile("sidt %0":"=m"(old_idt));
 uint16_t cs;__asm__ volatile("movw %%cs,%0":"=r"(cs));
 for(uint32_t v=0;v<256;v++)gate(v,irq807_unexpected,cs);
 gate(0x50,irq807_handler,cs);
 irq807_hits=0;irq807_isr_seen=0;irq807_eoi_clear=0;irq807_lapic_ptr=(volatile uint8_t *)lapic;
 arch_puts("STYLEOS S807 TOPOLOGY");fld("ioapic-physical",io);fld("ioapic-id",io_id);fld("ioapic-count",counts[0]);fld("gsi",plan.gsi);fld("pin",plan.pin);fld("lapic-physical",lapic);fld("lapic-id",dest);arch_putc('\n');
 arch_puts("STYLEOS S807 BEFORE");fld("ioapic-low",original_low);fld("ioapic-high",original_high);fld("tpr",original_tpr);fld("svr",original_svr);fld("if",0);arch_putc('\n');
 __asm__ volatile("lidt %0"::"m"(new_idt):"memory");
 mmio_write(lapic,0x80,0);mmio_write(lapic,0xf0,(original_svr&~255u)|0xffu|0x100u);
 io_write(io,reg,original_low|0x10000u); /* mask before rewriting destination */
 io_write(io,reg+1,dest<<24);
 io_write(io,reg,0x50); /* vector, active high, edge, unmasked */
 uint32_t actual_low=io_read(io,reg),actual_high=io_read(io,reg+1);
 arch_puts("STYLEOS S807 ROUTE");fld("vector",0x50);fld("ioapic-low",actual_low);fld("ioapic-high",actual_high);fld("gsi",plan.gsi);arch_putc('\n');
 if(actual_low!=0x50||actual_high!=(dest<<24))fail("ioapic-route-readback");
 /* PIT channel0, periodic mode2, 11932 ticks ~100Hz. Terminal isolated run.
  * PIC masks are NOT changed. Avoid unbounded HLT: use TSC timeout. */
 outb(0x43,0x34);outb(0x40,(uint8_t)11932);outb(0x40,(uint8_t)(11932>>8));
 uint64_t start=tsc();__asm__ volatile("sti" ::: "memory");
 uint64_t elapsed=0;
 while(irq807_hits<1 && (elapsed=tsc()-start)<UINT64_C(3000000000))__asm__ volatile("pause" ::: "memory");
 __asm__ volatile("cli" ::: "memory");
 elapsed=tsc()-start;
 /* Terminal restoration; PIT remains in programmed mode, no return to OS. */
 io_write(io,reg,0x10000u|0x50u);io_write(io,reg+1,original_high);io_write(io,reg,original_low);
 mmio_write(lapic,0x80,original_tpr);mmio_write(lapic,0xf0,original_svr);
 __asm__ volatile("lidt %0"::"m"(old_idt):"memory");
 uint32_t restored_low=io_read(io,reg),restored_high=io_read(io,reg+1);
 uint32_t restored_tpr=mmio_read(lapic,0x80),restored_svr=mmio_read(lapic,0xf0);
 arch_puts("STYLEOS S807 DELIVERY");fld("hits",irq807_hits);fld("isr-seen",irq807_isr_seen);fld("eoi-cleared",irq807_eoi_clear);fld("elapsed-tsc",elapsed);arch_putc('\n');
 arch_puts("STYLEOS S807 RESTORE");fld("ioapic-low",restored_low);fld("ioapic-high",restored_high);fld("tpr",restored_tpr);fld("svr",restored_svr);fld("if",0);arch_putc('\n');
 if(!irq807_hits||!irq807_isr_seen||!irq807_eoi_clear)fail("missing-delivery-or-eoi");
 if(original_low!=restored_low||original_high!=restored_high||original_tpr!=restored_tpr||original_svr!=restored_svr)fail("restore-mismatch");
 arch_puts("STYLEOS S807 POLICY madt=verified ioapic-route=verified pit-irq0-delivery=verified lapic-isr-eoi=verified ioapic-lapic-restored=yes pit-restored=no ordinary-desktop=unchanged dma-iommu=unverified uefi-runtime=unverified production-cr3=deny\n");
 arch_puts("STYLEOS S807 PASS\n");
 __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));
 arch_halt();
}
