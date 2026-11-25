#include "efi.h"
#include <styleos/acpi_madt_806.h>
#include <styleos/irq_route_807.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#ifdef STYLEOS_TEST_PHYSICAL_ARENA_844
void s844_guest_run(uint64_t);
#ifdef STYLEOS_TEST_USER_ROOTS_846
void s846_guest_run(uint64_t);
#endif
#endif
#ifdef STYLEOS_TEST_TIMER_SCHEDULER_843
#include <styleos/timer_context_843.h>
#include <styleos/apic_root_842.h>
#ifdef STYLEOS_TEST_TASK_STACKS_845
#include <styleos/task_stack_trace_845.h>
bool s845_extent(struct boot_range *);
struct s844_arena *s845_arena(void);
#endif
#endif
#include <stddef.h>
#ifdef STYLEOS_TEST_RUNTIME_IRQ_859
#include <styleos/runtime_irq_859.h>
static volatile uint64_t irq859_rip,irq859_cs,irq859_flags,irq859_rsp,irq859_wait_rsp;
extern const unsigned char irq859_wait_lo[],irq859_wait_hi[];
extern void irq859_wait(void);
extern uintptr_t irq859_publish_lapic(uintptr_t value);
static volatile uint64_t irq859_handler_lapic __attribute__((used));
#endif
/* ISOLATED TERMINAL QEMU ONLY. No code is linked to ordinary Desktop.
 * PIT0 -> ISA IRQ0 -> MADT ISO GSI2 -> IOAPIC -> LAPIC vector 0x50.
 * All MMIO, timer and IDT changes occur after ExitBootServices. */
static volatile uint32_t irq807_hits,irq807_isr_seen,irq807_eoi_clear;
#ifdef STYLEOS_TEST_RUNTIME_IRQ_859
static volatile uint8_t * volatile irq807_lapic_ptr __attribute__((used));
#else
static volatile uint8_t *irq807_lapic_ptr;
#endif
#ifdef STYLEOS_TEST_APIC_ROOT_842
static volatile uint64_t irq842_root_seen,irq842_frame_seen;
#endif
__asm__(
 ".global irq807_handler\n"
 "irq807_handler:\n"
 "pushq %rax\n"
 "pushq %rdx\n"
#ifdef STYLEOS_TEST_APIC_ROOT_842
 "movq %cr3,%rax\n"
 "movq %rax,irq842_root_seen(%rip)\n"
 "leaq 16(%rsp),%rax\n"
 "movq %rax,irq842_frame_seen(%rip)\n"
#endif
#ifdef STYLEOS_TEST_RUNTIME_IRQ_859
 "movq 16(%rsp),%rax\nmovq %rax,irq859_rip(%rip)\n"
 "movq 24(%rsp),%rax\nmovq %rax,irq859_cs(%rip)\n"
 "movq 32(%rsp),%rax\nmovq %rax,irq859_flags(%rip)\n"
 "movq 40(%rsp),%rax\nmovq %rax,irq859_rsp(%rip)\n"
#endif
 "incl irq807_hits(%rip)\n"
 "movq irq807_lapic_ptr(%rip),%rax\n"
#ifdef STYLEOS_TEST_RUNTIME_IRQ_859
 "movq %rax,irq859_handler_lapic(%rip)\n"
 "movl $0xfee00000,%edx\n"
 "cmpq %rdx,%rax\n"
 "jne irq807_unexpected\n"
#endif
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
#ifdef STYLEOS_TEST_TIMER_SCHEDULER_843
extern void s843_irq_entry(void);
extern _Noreturn void s843_enter_first(struct s843_frame *);
extern const unsigned char s843_workload_a[],s843_workload_a_end[],s843_workload_b[],s843_workload_b_end[];
extern volatile uint64_t s843_progress_a,s843_progress_b;
#ifdef STYLEOS_TEST_TASK_STACKS_845
static struct s845_pair pair845;
static struct s845_trace trace845;
static struct boot_range extent845;
static struct s844_arena *arena845;
#else
_Alignas(4096) static unsigned char stacks843[2][TASKS_STACK_BYTES];
#endif
static struct s843_scheduler sched843;
static struct {
 uintptr_t lapic,io;uint32_t reg,low,high,tpr,svr;
 struct idtr807 idt;
 uint64_t start,progress[2],root,ist;
 unsigned defers;
 bool admitted;
} hw843;
static uint64_t root843(void){uint64_t n;__asm__ volatile("mov %%cr3,%0":"=r"(n));return n;}
static bool rights843(const struct s842_root *r,struct boot_range a,unsigned flags){
 if(a.start>=a.end)return false;
 for(size_t i=0;i<r->plan.count;i++){
  const struct pg_mapping *m=&r->plan.maps[i];
  if(m->va==m->pa&&m->flags==flags&&m->va<=a.start&&a.end-m->va<=m->bytes)return true;
 }
 return false;
}
static struct boot_range extent843(const void *a,size_t n){
 return (struct boot_range){(uintptr_t)a,(uintptr_t)a+n};
}
bool s843_admit(const struct s842_root *r,struct pg_experiment *p,uint64_t root,uint64_t ist){
 if(!r||!p||!r->ready||!root||!ist||hw843.admitted)return false;
#ifdef STYLEOS_TEST_TASK_STACKS_845
 const struct s845_code code[2]={
 {{(uintptr_t)s843_workload_a,(uintptr_t)s843_workload_a_end},(uintptr_t)s843_workload_a},
 {{(uintptr_t)s843_workload_b,(uintptr_t)s843_workload_b_end},(uintptr_t)s843_workload_b}};
 if(!s845_extent(&extent845)||!rights843(r,extent845,PG_WRITE)||
    !rights843(r,code[0].span,PG_EXEC)||!rights843(r,code[1].span,PG_EXEC)||
    !rights843(r,extent843(&pair845,sizeof pair845),PG_WRITE)||
    !rights843(r,extent843(&trace845,sizeof trace845),PG_WRITE)||
    !rights843(r,extent843(&extent845,sizeof extent845),PG_WRITE)||
    !rights843(r,extent843(&arena845,sizeof arena845),PG_WRITE))return false;
#else
 const struct s843_task task[2]={
 {{(uintptr_t)s843_workload_a,(uintptr_t)s843_workload_a_end},extent843(stacks843[0],TASKS_STACK_BYTES),(uintptr_t)s843_workload_a},
 {{(uintptr_t)s843_workload_b,(uintptr_t)s843_workload_b_end},extent843(stacks843[1],TASKS_STACK_BYTES),(uintptr_t)s843_workload_b}};
 for(unsigned i=0;i<2;i++){
  if(!rights843(r,task[i].code,PG_EXEC)||!rights843(r,task[i].stack,PG_WRITE)||
     arch_paging_attest_image_writable(p,task[i].stack.start,TASKS_STACK_BYTES)!=PG_OK)return false;
 }
 #endif
 if(!rights843(r,extent843(&sched843,sizeof sched843),PG_WRITE)||
    !rights843(r,extent843(&hw843,sizeof hw843),PG_WRITE)||
    !rights843(r,extent843((const void *)&s843_progress_a,8),PG_WRITE)||
    !rights843(r,extent843((const void *)&s843_progress_b,8),PG_WRITE))return false;
#ifndef STYLEOS_TEST_TASK_STACKS_845
 uint16_t cs,ss;__asm__ volatile("mov %%cs,%0;mov %%ss,%1":"=r"(cs),"=r"(ss));
 if(!s843_init(&sched843,task,(struct boot_range){ist,ist+4096},root,cs,ss))return false;
#endif
 hw843.root=root;hw843.ist=ist;hw843.admitted=true;return true;
}
static uint64_t pattern843(unsigned task,unsigned i){return UINT64_C(0x8430000000000000)|((uint64_t)task<<32)|i;}
static uint64_t reg843(const struct s843_frame *f,unsigned i){
 const unsigned char *p=(const unsigned char *)f+8u*i;uint64_t value=0;
 for(unsigned j=0;j<8;j++)value|=(uint64_t)p[j]<<(8u*j);
 return value;
}
static void seed843(struct s843_frame *f,unsigned task){
 unsigned char *p=(unsigned char *)f;
 for(unsigned i=0;i<15;i++)for(unsigned j=0;j<8;j++)p[8*i+j]=(unsigned char)(pattern843(task,i)>>(8*j));
}
static bool regs843(const struct s843_frame *f,unsigned task){for(unsigned i=0;i<15;i++)if(reg843(f,i)!=pattern843(task,i))return false;return true;}
#ifdef STYLEOS_TEST_TASK_STACKS_845
static void start845(uint64_t root,uint64_t ist){
 uint16_t cs,ss;uint64_t flags;
 __asm__ volatile("mov %%cs,%0;mov %%ss,%1;pushfq;popq %2":"=r"(cs),"=r"(ss),"=r"(flags));
 const struct s845_code code[2]={
 {{(uintptr_t)s843_workload_a,(uintptr_t)s843_workload_a_end},(uintptr_t)s843_workload_a},
 {{(uintptr_t)s843_workload_b,(uintptr_t)s843_workload_b_end},(uintptr_t)s843_workload_b}};
 arena845=s845_arena();
 if(!arena845||arena845->base!=extent845.start||extent845.end!=extent845.start+S844_BYTES||root843()!=root||flags&0x200||
    !s845_prepare(arena845,&pair845,&sched843,code,(struct boot_range){ist,ist+4096},root,cs,ss))fail("s845-leases");
 arch_puts("STYLEOS S845 LEASE");fld("base",extent845.start);fld("end",extent845.end);
 fld("a",pair845.lease[0].pa);fld("b",pair845.lease[1].pa);fld("gen-a",pair845.lease[0].generation);fld("gen-b",pair845.lease[1].generation);arch_putc('\n');
}
static void finish845(struct s843_frame *f){
 uint64_t rsp,flags;__asm__ volatile("mov %%rsp,%0;pushfq;popq %1":"=r"(rsp),"=r"(flags));
 if(trace845.ticks!=16||!s845_release_terminal(arena845,&pair845,&sched843,f,rsp,root843(),flags))fail("s845-terminal-release");
 unsigned used=99;
 if(!s844_count(arena845,&used)||used||pair845.live||sched843.ready)fail("s845-live-after-release");
 const volatile unsigned char *data=(const volatile unsigned char *)(uintptr_t)extent845.start;
 for(size_t i=0;i<2*TASKS_STACK_BYTES;i++)if(data[i])fail("s845-not-zero");
 struct s844_lease reuse;
 if(!s844_acquire(arena845,0x8453,8,&reuse)||reuse.pa!=extent845.start||reuse.generation!=6||!s844_release(arena845,0x8453,reuse)||!s844_count(arena845,&used)||used)fail("s845-reuse");
 arch_puts("STYLEOS S845 RELEASE");fld("root",root843());fld("rsp",rsp);fld("frame",(uintptr_t)f);fld("flags",flags);
 fld("a",trace845.count[0]);fld("b",trace845.count[1]);fld("used",used);fld("zero",1);fld("reuse-gen",reuse.generation);arch_putc('\n');
 arch_puts("STYLEOS S845 POLICY terminal=yes contexts-cleared=yes cpl3=no production-cr3=deny\n");
}
#endif
#ifdef STYLEOS_TEST_TASK_STACKS_845
static _Noreturn void finish843(uint64_t frame){
 uint64_t elapsed=tsc()-hw843.start;
 const uint64_t ticks=sched843.ticks,dispatches=sched843.control.dispatches;
 if(ticks!=16||sched843.control.slots[0].yields!=8||sched843.control.slots[1].yields!=8)fail("s843-tick-total");
 io_write(hw843.io,hw843.reg,0x10050u);
#ifdef STYLEOS_TEST_TASK_STACKS_845
 finish845((struct s843_frame *)(uintptr_t)frame);
#endif
 io_write(hw843.io,hw843.reg+1,hw843.high);io_write(hw843.io,hw843.reg,hw843.low);
 mmio_write(hw843.lapic,0x80,hw843.tpr);mmio_write(hw843.lapic,0xf0,hw843.svr);
 __asm__ volatile("lidt %0"::"m"(hw843.idt):"memory");
 uint32_t low=io_read(hw843.io,hw843.reg),high=io_read(hw843.io,hw843.reg+1),tpr=mmio_read(hw843.lapic,0x80),svr=mmio_read(hw843.lapic,0xf0);
 if(low!=hw843.low||high!=hw843.high||tpr!=hw843.tpr||svr!=hw843.svr||root843()!=hw843.root)fail("s843-finish");
 arch_puts("STYLEOS S807 DELIVERY");fld("hits",ticks+hw843.defers);fld("isr-seen",1);fld("eoi-cleared",1);fld("elapsed-tsc",elapsed);arch_putc('\n');
 arch_puts("STYLEOS S807 RESTORE");fld("ioapic-low",low);fld("ioapic-high",high);fld("tpr",tpr);fld("svr",svr);fld("if",0);arch_putc('\n');
 arch_puts("STYLEOS S842 IRQ");fld("before",hw843.root);fld("during",root843());fld("after",root843());fld("frame",frame+120);fld("ist",hw843.ist);arch_putc('\n');
 arch_puts("STYLEOS S842 POLICY isolated-own-root=yes pit-restored=no production-cr3=deny dma-grants=no\n");
 arch_puts("STYLEOS S843 TOTAL");fld("ticks",ticks);fld("dispatches",dispatches);fld("a",s843_progress_a);fld("b",s843_progress_b);arch_putc('\n');
 arch_puts("STYLEOS S843 POLICY integer-only=yes cpl3=no production-cr3=deny pit-restored=no\n");
 arch_puts("STYLEOS S807 POLICY madt=verified ioapic-route=verified pit-irq0-delivery=verified lapic-isr-eoi=verified ioapic-lapic-restored=yes pit-restored=no ordinary-desktop=unchanged dma-iommu=unverified uefi-runtime=unverified production-cr3=deny\n");
 arch_puts("STYLEOS S807 PASS\n");
 __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));arch_halt();
}
#else
static _Noreturn void finish843(uint64_t frame){
 uint64_t elapsed=tsc()-hw843.start;
 io_write(hw843.io,hw843.reg,0x10050u);io_write(hw843.io,hw843.reg+1,hw843.high);io_write(hw843.io,hw843.reg,hw843.low);
 mmio_write(hw843.lapic,0x80,hw843.tpr);mmio_write(hw843.lapic,0xf0,hw843.svr);
 __asm__ volatile("lidt %0"::"m"(hw843.idt):"memory");
 uint32_t low=io_read(hw843.io,hw843.reg),high=io_read(hw843.io,hw843.reg+1),tpr=mmio_read(hw843.lapic,0x80),svr=mmio_read(hw843.lapic,0xf0);
 if(low!=hw843.low||high!=hw843.high||tpr!=hw843.tpr||svr!=hw843.svr||root843()!=hw843.root||sched843.ticks!=16||sched843.control.slots[0].yields!=8||sched843.control.slots[1].yields!=8)fail("s843-finish");
 arch_puts("STYLEOS S807 DELIVERY");fld("hits",sched843.ticks+hw843.defers);fld("isr-seen",1);fld("eoi-cleared",1);fld("elapsed-tsc",elapsed);arch_putc('\n');
 arch_puts("STYLEOS S807 RESTORE");fld("ioapic-low",low);fld("ioapic-high",high);fld("tpr",tpr);fld("svr",svr);fld("if",0);arch_putc('\n');
 arch_puts("STYLEOS S842 IRQ");fld("before",hw843.root);fld("during",root843());fld("after",root843());fld("frame",frame+120);fld("ist",hw843.ist);arch_putc('\n');
 arch_puts("STYLEOS S842 POLICY isolated-own-root=yes pit-restored=no production-cr3=deny dma-grants=no\n");
 arch_puts("STYLEOS S843 TOTAL");fld("ticks",sched843.ticks);fld("dispatches",sched843.control.dispatches);fld("a",s843_progress_a);fld("b",s843_progress_b);arch_putc('\n');
 arch_puts("STYLEOS S843 POLICY integer-only=yes cpl3=no production-cr3=deny pit-restored=no\n");
 arch_puts("STYLEOS S807 POLICY madt=verified ioapic-route=verified pit-irq0-delivery=verified lapic-isr-eoi=verified ioapic-lapic-restored=yes pit-restored=no ordinary-desktop=unchanged dma-iommu=unverified uefi-runtime=unverified production-cr3=deny\n");
 arch_puts("STYLEOS S807 PASS\n");
 __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));arch_halt();
}
#endif
bool s843_irq_dispatch(struct s843_frame *f){
 uint64_t root=root843();unsigned cur=(unsigned)sched843.control.current;
 uint64_t a=s843_progress_a,b=s843_progress_b;
 if(!hw843.admitted||cur>1||root!=hw843.root||!f||(uintptr_t)f!=hw843.ist+4096-16-sizeof *f)fail("s843-irq-frame-root");
 if(!regs843(f,cur))fail("s843-irq-gprs");
 if(!(mmio_read(hw843.lapic,0x120)&0x10000u))fail("s843-irq-isr");
 const uint64_t now[2]={a,b};
 enum s843_irq_action action=s843_irq_progress(&sched843,f,root,hw843.progress,now,hw843.defers);
 if(action==S843_IRQ_BAD)fail("s843-irq-progress-or-context");
 if(action==S843_IRQ_DEFER){
  /* Same context, no schedule/stack accounting change. Require a real
   * in-service IRQ and successful EOI even for a no-progress interrupt. */
  mmio_write(hw843.lapic,0xb0,0);
  if(mmio_read(hw843.lapic,0x120)&0x10000u)fail("s843-defer-eoi");
  hw843.defers++;
  arch_puts("STYLEOS S843 DEFER");fld("n",sched843.ticks+1);fld("count",hw843.defers);fld("from",cur);fld("root",root);
  fld("frame",(uintptr_t)f);fld("rip",f->rip);fld("rsp",f->rsp);fld("a",a);fld("b",b);fld("gprs",0x7fff);fld("eoi",1);
  const char *const names[15]={"r15","r14","r13","r12","r11","r10","r9","r8","rdi","rsi","rbp","rbx","rdx","rcx","rax"};
  for(unsigned i=0;i<15;i++)fld(names[i],reg843(f,i));arch_putc('\n');
  return true;
 }
#ifdef STYLEOS_TEST_TASK_STACKS_845
 if(!s845_observe(&trace845,&pair845,&sched843,f,a,b))fail("s845-stack-trace");
 arch_puts("STYLEOS S845 STACK");fld("n",trace845.ticks);fld("from",cur);fld("rsp",f->rsp);fld("a",trace845.count[0]);fld("b",trace845.count[1]);arch_putc('\n');
#endif
 struct s843_frame interrupted=*f;
 uint64_t rip=f->rip,rsp=f->rsp;
 if(!s843_tick(&sched843,f,root)||!regs843(f,1-cur))fail("s843-switch");
 mmio_write(hw843.lapic,0xb0,0);
 if(mmio_read(hw843.lapic,0x120)&0x10000u)fail("s843-eoi");
 hw843.progress[0]=a;hw843.progress[1]=b;
 arch_puts("STYLEOS S843 TICK");fld("n",sched843.ticks);fld("from",cur);fld("to",1-cur);fld("root",root);
 fld("frame",(uintptr_t)f);fld("rip",rip);fld("rsp",rsp);fld("a",a);fld("b",b);fld("gprs",0x7fff);fld("eoi",1);
 const char *const names[15]={"r15","r14","r13","r12","r11","r10","r9","r8","rdi","rsi","rbp","rbx","rdx","rcx","rax"};
 for(unsigned i=0;i<15;i++)fld(names[i],reg843(&interrupted,i));arch_putc('\n');
 if(sched843.ticks==16)finish843((uintptr_t)f);
 return true;
}
#endif
static _Noreturn void irq_run(const struct boot_info *b
#ifdef STYLEOS_TEST_APIC_ROOT_842
,uint64_t expected_root,uint64_t ist
#endif
){
#ifdef STYLEOS_TEST_APIC_ROOT_842
 uint64_t initial;__asm__ volatile("mov %%cr3,%0":"=r"(initial));
 if(!expected_root||initial!=expected_root||!ist)fail("s842-initial-root");
#ifdef STYLEOS_TEST_PHYSICAL_ARENA_844
 s844_guest_run(expected_root);
#ifdef STYLEOS_TEST_USER_ROOTS_846
 s846_guest_run(expected_root);
#endif
#ifdef STYLEOS_TEST_TASK_STACKS_845
 start845(expected_root,ist);
#endif
#endif
#endif
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
#ifdef STYLEOS_TEST_TIMER_SCHEDULER_843
 gate(0x50,s843_irq_entry,cs);
#else
 gate(0x50,irq807_handler,cs);
#endif
#ifdef STYLEOS_TEST_APIC_ROOT_842
 for(unsigned v=0;v<256;v++)idt[v].ist=3;
 idt[0x50].ist=1;irq842_root_seen=0;irq842_frame_seen=0;
#endif
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
#ifdef STYLEOS_TEST_TIMER_SCHEDULER_843
 io_write(io,reg,0x10050u);
 if(!hw843.admitted||hw843.root!=expected_root||hw843.ist!=ist)fail("s843-not-admitted");
 hw843.lapic=lapic;hw843.io=io;hw843.reg=reg;hw843.low=original_low;hw843.high=original_high;
 hw843.tpr=original_tpr;hw843.svr=original_svr;hw843.idt=old_idt;
 s843_progress_a=0;s843_progress_b=0;
 for(unsigned i=0;i<2;i++)seed843(&sched843.saved[i],i);
 struct s843_frame *first=(void *)(uintptr_t)(ist+4096-16-sizeof(struct s843_frame));
 if(!s843_first(&sched843,first))fail("s843-first-frame");
 arch_puts("STYLEOS S843 READY");fld("root",expected_root);fld("ist",ist);
 for(unsigned i=0;i<2;i++){fld(i?"b-code":"a-code",sched843.task[i].code.start);fld(i?"b-end":"a-end",sched843.task[i].code.end);fld(i?"b-stack":"a-stack",sched843.task[i].stack.start);}
 arch_putc('\n');
 /* 20Hz leaves time for a complete UART observation before the next tick. */
 outb(0x43,0x34);outb(0x40,(uint8_t)59659);outb(0x40,(uint8_t)(59659>>8));
 io_write(io,reg,0x50u);
 hw843.start=tsc();s843_enter_first(first);
#else
 outb(0x43,0x34);outb(0x40,(uint8_t)11932);outb(0x40,(uint8_t)(11932>>8));
#endif
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
#ifdef STYLEOS_TEST_APIC_ROOT_842
 uint64_t final;__asm__ volatile("mov %%cr3,%0":"=r"(final));
 if(final!=expected_root||irq842_root_seen!=expected_root||irq842_frame_seen<ist||
 irq842_frame_seen>ist+4096-40||(irq842_frame_seen&7u))fail("s842-irq-root-or-ist");
 arch_puts("STYLEOS S842 IRQ");fld("before",initial);fld("during",irq842_root_seen);fld("after",final);
 fld("frame",irq842_frame_seen);fld("ist",ist);arch_putc('\n');
 arch_puts("STYLEOS S842 POLICY isolated-own-root=yes pit-restored=no production-cr3=deny dma-grants=no\n");
#endif
 arch_puts("STYLEOS S807 POLICY madt=verified ioapic-route=verified pit-irq0-delivery=verified lapic-isr-eoi=verified ioapic-lapic-restored=yes pit-restored=no ordinary-desktop=unchanged dma-iommu=unverified uefi-runtime=unverified production-cr3=deny\n");
 arch_puts("STYLEOS S807 PASS\n");
 __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));
 arch_halt();
}

#ifdef STYLEOS_TEST_APIC_ROOT_842
_Noreturn void irq842_terminal(const struct boot_info *b,uint64_t root,uint64_t ist){irq_run(b,root,ist);}
#else
_Noreturn void irq807_terminal(const struct boot_info *b){irq_run(b);}
#endif

#ifdef STYLEOS_TEST_RUNTIME_IRQ_859
/* Only STI inside a bounded leaf loop; no C call or scheduler mutation with IF=1.
 * The already-tested irq807_handler captures the interrupted root/IST and EOI. */
__asm__(".text\n.global irq859_publish_lapic\nirq859_publish_lapic:\n"
 "movq %rcx,irq807_lapic_ptr(%rip)\n"
 "movq irq807_lapic_ptr(%rip),%rax\nretq\n"
 ".global irq859_wait\nirq859_wait:\n"
 "movq %rsp,irq859_wait_rsp(%rip)\nmovl $5000000,%ecx\n"
 "1: sti\n.global irq859_wait_lo\nirq859_wait_lo: nop\ncli\n.global irq859_wait_hi\nirq859_wait_hi:\n"
 "cmpl $0,irq807_hits(%rip)\njne 2f\ndecl %ecx\njnz 1b\n2: retq\n");
static uint64_t irq859_root(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static uint64_t irq859_rflags(void){uint64_t v;__asm__ volatile("pushfq;popq %0":"=r"(v));return v;}
static bool irq859_clear(uintptr_t lapic){
 for(unsigned i=0;i<8;i++)if(mmio_read(lapic,0x100+16*i)||mmio_read(lapic,0x200+16*i))return false;
 return true;
}
static _Alignas(16) struct idt_gate807 irq859_saved[256];
static _Noreturn void irq859_fail(const char *reason){
 __asm__ volatile("cli":::"memory");arch_puts("STYLEOS S859 FAIL ");arch_puts(reason);arch_putc('\n');
 __asm__ volatile("outl %0,%1"::"a"(0x11u),"Nd"((uint16_t)0xf4u));arch_halt();
}
static void irq859_lvt_read(uintptr_t lapic,uint32_t *values){
 for(unsigned i=0;i<IRQ859_LVT_COUNT;i++)values[i]=mmio_read(lapic,0x320+16*i);
}
static void irq859_lvt_log(const char *tag,uint64_t root,uint32_t version,const uint32_t *values){
 const char *keys[]={"timer","thermal","perf","lint0","lint1","error"};
 arch_puts("STYLEOS S859 ");arch_puts(tag);fld("root",root);fld("version",version);
 for(unsigned i=0;i<IRQ859_LVT_COUNT;i++){fld(keys[i],values[i]);}arch_putc('\n');
}
void irq859_once(uint64_t root,uint64_t idt_pa,uint16_t cs,uint64_t stack_lo,uint64_t stack_hi){
 struct idtr807 dt;__asm__ volatile("sidt %0":"=m"(dt));
 if(irq859_root()!=root||(irq859_rflags()&0x200)||dt.base!=idt_pa||dt.limit!=4095||inb(0x21)!=255||inb(0xa1)!=255)irq859_fail("859-context");
 struct acpi806_topology t={0};
 if(!acpi806_copy_capture(&t)||t.enabled_lapics!=1||t.ioapic_count!=1||t.lapic_physical!=0xfee00000u||t.ioapic[0].physical!=0xfec00000u)irq859_fail("859-madt");
 uintptr_t lapic=t.lapic_physical,io=t.ioapic[0].physical;
 if((rdmsr(0x1b)&UINT64_C(0xfffffc00))!=UINT64_C(0xfee00800))irq859_fail("859-xapic");
 uint32_t index=mmio_read(io,0),ioid=(io_read(io,0)>>24)&15u,version=io_read(io,1),counts[ACPI806_MAX_IOAPIC]={0};
 counts[0]=((version>>16)&255u)+1u;
 struct irq807_plan plan={0};
 if(ioid!=t.ioapic[0].id||counts[0]!=24||!irq807_select_pit(&t,counts,&plan)||plan.ioapic_index||plan.gsi!=2||plan.pin!=2||!irq859_clear(lapic))irq859_fail("859-route");
 uint32_t low[24],high[24];
 for(unsigned i=0;i<24;i++){low[i]=io_read(io,0x10+2*i);high[i]=io_read(io,0x11+2*i);if(!(low[i]&0x10000u)){arch_puts("STYLEOS S859 BLOCKED");fld("pin",i);fld("low",low[i]);arch_putc('\n');irq859_fail("859-other-irq-unmasked");}}
 /* Observe the complete supported LVT bank before any source change. */
 uint32_t lvt_version=mmio_read(lapic,0x30),lvt_before[IRQ859_LVT_COUNT],lvt_masked[IRQ859_LVT_COUNT],lvt_after[IRQ859_LVT_COUNT],lvt_plan[IRQ859_LVT_COUNT];
 if(((lvt_version>>16)&255u)!=5u){arch_puts("STYLEOS S859 BLOCKED");fld("lvt-version",lvt_version);arch_putc('\n');irq859_fail("859-lvt-version-count");}
 irq859_lvt_read(lapic,lvt_before);irq859_lvt_log("LVT-BEFORE",root,lvt_version,lvt_before);
 if(!irq859_lvt_plan(lvt_version,lvt_before,lvt_plan))irq859_fail("859-lvt-plan");
 uint32_t dest=mmio_read(lapic,0x20)>>24,tpr=mmio_read(lapic,0x80),svr=mmio_read(lapic,0xf0),reg=0x10+2*plan.pin;
 if(dest!=0||low[2]!=0x10000u||high[2]!=0||tpr!=0||svr!=0x10fu){arch_puts("STYLEOS S859 BLOCKED");fld("dest",dest);fld("low",low[2]);fld("high",high[2]);fld("tpr",tpr);fld("svr",svr);arch_putc('\n');irq859_fail("859-initial-registers");}
 struct idt_gate807 *live=(void *)(uintptr_t)idt_pa;
 for(unsigned i=0;i<256;i++)irq859_saved[i]=live[i];
 const uintptr_t handler=(uintptr_t)irq807_handler;
 live[IRQ859_VECTOR]=(struct idt_gate807){(uint16_t)handler,cs,1,0x8e,(uint16_t)(handler>>16),(uint32_t)(handler>>32),0};
 irq807_hits=irq807_isr_seen=irq807_eoi_clear=0;irq842_root_seen=irq842_frame_seen=0;irq859_rip=irq859_cs=irq859_flags=irq859_rsp=irq859_wait_rsp=0;
 irq859_handler_lapic=0;
 /* Explicit MS x64 ABI leaf; compiler cannot elide the publication. */
 uintptr_t published=irq859_publish_lapic(lapic);
 if(published!=lapic||(uintptr_t)irq807_lapic_ptr!=lapic)irq859_fail("859-lapic-publish");
 /* Terminal fixture: mask LINT1 NMI first, then LINT0 ExtINT. IF=0
  * alone does not block NMI; unexpected events are terminal failures. */
 if(lvt_before[4]!=lvt_plan[4])mmio_write(lapic,0x360,lvt_plan[4]);
 if(lvt_before[3]!=lvt_plan[3])mmio_write(lapic,0x350,lvt_plan[3]);
 irq859_lvt_read(lapic,lvt_masked);irq859_lvt_log("LVT-MASKED",root,mmio_read(lapic,0x30),lvt_masked);
 for(unsigned i=0;i<IRQ859_LVT_COUNT;i++)if(lvt_masked[i]!=lvt_plan[i])irq859_fail("859-lvt-mask-readback");
 if(mmio_read(lapic,0x30)!=lvt_version||!irq859_clear(lapic))irq859_fail("859-lvt-mask-pending");
 mmio_write(lapic,0xf0,0x1ffu);
 io_write(io,reg,0x10050u);io_write(io,reg+1,dest<<24);
 /* Mode 0, single terminal-count edge. Old PIT count/mode NOT restored. */
 outb(0x43,0x30);outb(0x40,(uint8_t)11932);outb(0x40,(uint8_t)(11932>>8));
 io_write(io,reg,0x50u);
 if(io_read(io,reg)!=0x50u||io_read(io,reg+1)!=0||mmio_read(lapic,0xf0)!=0x1ffu)irq859_fail("859-route-readback");
 irq859_wait();
 io_write(io,reg,0x10050u); /* no source delivery while restoring */
 bool drained=irq859_clear(lapic);
 uintptr_t cleared=irq859_publish_lapic(0);
 if(cleared||(uintptr_t)irq807_lapic_ptr||irq859_handler_lapic!=lapic)irq859_fail("859-lapic-retire");
 io_write(io,reg+1,high[2]);io_write(io,reg,low[2]);mmio_write(lapic,0xf0,svr);
 live[IRQ859_VECTOR]=irq859_saved[IRQ859_VECTOR];
 /* The test route is masked/restored; IF remains clear. Restore LINT0
  * and then LINT1, with the original IDT already restored. */
 if(!drained||(irq859_rflags()&0x200)||!irq859_clear(lapic))irq859_fail("859-lvt-restore-context");
 if(lvt_before[3]!=lvt_plan[3])mmio_write(lapic,0x350,lvt_before[3]);
 if(lvt_before[4]!=lvt_plan[4])mmio_write(lapic,0x360,lvt_before[4]);
 irq859_lvt_read(lapic,lvt_after);irq859_lvt_log("LVT-RESTORED",root,mmio_read(lapic,0x30),lvt_after);
 for(unsigned i=0;i<IRQ859_LVT_COUNT;i++)if(lvt_after[i]!=lvt_before[i])irq859_fail("859-lvt-restore-readback");
 if(mmio_read(lapic,0x30)!=lvt_version)irq859_fail("859-lvt-version");
 for(unsigned i=0;i<24;i++)if(io_read(io,0x10+2*i)!=low[i]||io_read(io,0x11+2*i)!=high[i])irq859_fail("859-ioapic-restore");
 mmio_write(io,0,index);
 const unsigned char *a=(const void *)live,*b=(const void *)irq859_saved;
 for(unsigned i=0;i<4096;i++)if(a[i]!=b[i])irq859_fail("859-idt-restore");
 __asm__ volatile("sidt %0":"=m"(dt));
 struct irq859_expected e={root,IRQ859_IST,cs,(uintptr_t)irq859_wait_lo,(uintptr_t)irq859_wait_hi,irq859_wait_rsp};
 struct irq859_observed o={irq842_root_seen,irq842_frame_seen,irq859_cs,irq859_rip,irq859_rsp,irq859_flags,irq807_hits,irq807_isr_seen,irq807_eoi_clear};
 if(!drained||!irq859_clear(lapic)||!irq859_frame_ok(&e,&o)||e.rsp<stack_lo||e.rsp>=stack_hi||irq859_root()!=root||irq859_rflags()&0x200||dt.base!=idt_pa||dt.limit!=4095||mmio_read(io,0)!=index||mmio_read(lapic,0x80)!=tpr||mmio_read(lapic,0xf0)!=svr||inb(0x21)!=255||inb(0xa1)!=255){arch_puts("STYLEOS S859 BLOCKED");fld("hits",o.hits);fld("isr",o.isr);fld("eoi",o.eoi);fld("rip",o.rip);fld("rsp",o.rsp);fld("wait-rsp",e.rsp);fld("frame",o.frame);fld("flags",o.rflags);fld("drained",drained);arch_putc('\n');irq859_fail("859-final-proof");}
 arch_puts("STYLEOS S859 IRQ");fld("root",root);fld("idt",idt_pa);fld("ist",e.ist);fld("frame",o.frame);fld("cs",o.cs);fld("rip",o.rip);fld("wait-lo",e.wait_lo);fld("wait-hi",e.wait_hi);fld("rsp",o.rsp);fld("wait-rsp",e.rsp);fld("flags",o.rflags);fld("hits",o.hits);fld("isr",o.isr);fld("eoi",o.eoi);fld("pin",plan.pin);fld("low",low[2]);fld("high",high[2]);fld("svr",svr);fld("tpr",tpr);fld("drained",1);fld("published",published);fld("handler-lapic",irq859_handler_lapic);fld("cleared",cleared);arch_putc('\n');
}
#endif
