/* StyleOS 786: terminal, one-shot hardware IST1 experiment. Not Desktop. */
#include <styleos/ist_hw_trial_786.h>
#include <styleos/ist_guard_plan_785.h>
#include <styleos/tss_descriptor_783.h>
#include <styleos/tss_ist_stage_782.h>
#include <styleos/paging.h>
#include <styleos/arch.h>

#ifdef STYLEOS_TEST_IST_HW_786
#define PAGE 4096u
#define LONG_MODE_LIMIT (UINT64_C(1)<<47)
#define IST_MARKER UINT64_C(0x78615a0cafe12345)

/* Exactly nine mutually distinct, loaded-image, page-aligned frames. */
_Alignas(PAGE) static unsigned char frames[IGP785_FRAMES][PAGE];
struct desc_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));
struct idt_gate_786 {
    uint16_t off0, selector;
    uint8_t ist, attributes;
    uint16_t off1;
    uint32_t off2, reserved;
} __attribute__((packed));
_Static_assert(sizeof(struct idt_gate_786)==16u,"x86 IDT gate width");
_Static_assert(sizeof(struct desc_ptr)==10u,"descriptor pointer width");
_Static_assert(sizeof(struct igp785_tables)==4u*PAGE,"exact table pages");

struct state786 {
    uint64_t old_cr3, new_cr3;
    uint64_t boot_stack_start, boot_stack_end;
    uint64_t old_rsp_before;
    uint64_t idt_base,gdt_base;
    uint16_t cs, tr_selector;
    uint64_t ready;
};
static struct state786 attested;

static uint64_t cr3(void) { uint64_t n; __asm__ volatile("mov %%cr3,%0":"=r"(n));return n; }
static uint64_t cr0(void) { uint64_t n; __asm__ volatile("mov %%cr0,%0":"=r"(n));return n; }
static uint64_t cr4(void) { uint64_t n; __asm__ volatile("mov %%cr4,%0":"=r"(n));return n; }
static uint64_t rflags(void) { uint64_t n; __asm__ volatile("pushfq;popq %0":"=r"(n));return n; }
static uint16_t current_cs(void) { uint16_t n; __asm__ volatile("mov %%cs,%0":"=r"(n));return n; }
static uint16_t current_tr(void) { uint16_t n; __asm__ volatile("str %0":"=r"(n));return n; }
static uint64_t efer(void) { uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000080u));return ((uint64_t)hi<<32u)|lo; }
static void field(const char *s,uint64_t v) { arch_puts(s);paging_hex(v); }
__attribute__((noreturn)) static void stop_vm(unsigned status) {
    __asm__ volatile("cli;outl %0,%1"::"a"(status),"Nd"((uint16_t)0xf4):"memory");
    arch_halt();
}
__attribute__((noreturn)) static void fail(const char *why) {
    arch_puts("STYLEOS S786 FAIL ");arch_puts(why);arch_putc('\n');stop_vm(0x11u);
}
extern void s786_after_int3(void);
__attribute__((noreturn,noinline,used,naked)) static void s786_int3(void) {
    __asm__ volatile("int3\n.globl s786_after_int3\ns786_after_int3:\nud2\n");
}
/* Capture the ORIGINAL interrupt RSP before the Microsoft x64 call frame.
 * Do not use a returning handler: no UEFI, IRET, or test CPU reset. */
__attribute__((noreturn,noinline,used)) void s786_on_interrupt(uint64_t cpu_rsp);
__attribute__((noreturn,noinline,used,naked)) static void s786_isr3(void) {
    __asm__ volatile("cli\nmovq %rsp,%rcx\nandq $-16,%rsp\nsubq $32,%rsp\ncall s786_on_interrupt\nud2\n");
}
static uint64_t read64(const void *source) {
    const unsigned char *p=(const unsigned char*)source;
    uint64_t v=0;
    for(unsigned i=0;i<8;++i) v|=(uint64_t)p[i]<<(8u*i);
    return v;
}
__attribute__((noreturn,noinline,used)) void s786_on_interrupt(uint64_t cpu_rsp) {
    const struct igp785_tables *t=(const struct igp785_tables*)(const void*)frames[IGP785_NEW_PML4];
    const uint64_t low=IGP785_IST_ALIAS, high=low+PAGE;
    struct desc_ptr idtr={0},gdtr={0};
    __asm__ volatile("sidt %0;sgdt %1":"=m"(idtr),"=m"(gdtr)::"memory");
    if(attested.ready!=IST_MARKER || cr3()!=attested.new_cr3 ||
       (cr0() & (UINT64_C(1)<<16u))==0 ||
       (efer() & (UINT64_C(1)<<11u))==0 ||
       (rflags() & (UINT64_C(1)<<9u))!=0 ||
       current_cs()!=attested.cs || current_tr()!=attested.tr_selector ||
       idtr.base!=attested.idt_base || idtr.limit!=4095u ||
       gdtr.base!=attested.gdt_base ||
       cpu_rsp<low || cpu_rsp>high-5u*8u ||
       (cpu_rsp & 7u)!=0 ||
       t->pml4[128]!=(uint64_t)(uintptr_t)frames[IGP785_NEW_PDPT]+3u ||
       t->pdpt[0]!=(uint64_t)(uintptr_t)frames[IGP785_NEW_PD]+3u ||
       t->pd[0]!=(uint64_t)(uintptr_t)frames[IGP785_NEW_PT]+3u ||
       t->pt[2] || t->pt[4] ||
       t->pt[3]!=((uint64_t)(uintptr_t)frames[IGP785_IST1]|IGP785_STACK_PTE) ||
       *(volatile uint64_t*)(uintptr_t)(low+0x60u)!=IST_MARKER)
        fail("live-root-or-ist-layout");
    const uint64_t *hardware=(const uint64_t*)(uintptr_t)cpu_rsp;
    const uint64_t rip=hardware[0],cs=hardware[1],flags=hardware[2];
    const uint64_t old_rsp=hardware[3];
    if(rip!=(uint64_t)(uintptr_t)&s786_after_int3 || cs!=attested.cs ||
       !(flags&(UINT64_C(1)<<1u)) ||
       old_rsp<attested.boot_stack_start || old_rsp>=attested.boot_stack_end)
        fail("hardware-frame");
    field("STYLEOS S786 TRAP old=",attested.old_cr3);
    field(" new=",cr3());field(" frame-rsp=",cpu_rsp);
    field(" saved-rip=",rip);
    field(" expected-rip=",(uint64_t)(uintptr_t)&s786_after_int3);
    field(" saved-cs=",cs);field(" tr=",current_tr());
    field(" ist-top=",IGP785_IST_ALIAS+PAGE-16u);
    field(" leaf=",t->pt[3]);
    arch_puts(" guard-lo=0 guard-hi=0 pt-count=1 cr3=1 marker=1 hardware-int3=1\n");
    arch_puts("STYLEOS S786 TERMINAL qemu-exit=0x10 firmware-return=0 guard-fault-injected=0\n");
    stop_vm(0x10u);
}

__attribute__((noreturn)) void s786_terminal_trial(struct pg_experiment *p) {
    if(!p || !p->evidence || !p->evidence->map || !p->old_root ||
       cr3()!=p->old_root || current_tr()!=0 || (rflags()&(1u<<9u)) ||
       (efer()&(UINT64_C(1)<<11u))==0 ||
       (cr0()&(UINT64_C(1)<<16u))==0 ||
       (cr4()&(UINT64_C(1)<<12u))!=0)
        fail("inherited-cpu-context");
    const struct boot_memory_evidence *e=p->evidence;
    if(e->map->nr_candidates>IGP785_MAX_EXCLUSIONS)fail("candidate-capacity");
    struct igp785_request q={0};
    q.reserved_image=e->live.image;
    q.old_cr3_pa=p->old_root;
    q.virtual_ist=IGP785_IST_ALIAS;
    for(size_t i=0;i<IGP785_FRAMES;++i) {
        q.pages[i]=(uint64_t)(uintptr_t)frames[i];
        if(arch_paging_attest_image_writable(p,q.pages[i],PAGE)!=PG_OK)
            fail("physical-frame-attestation");
    }
    q.nr_candidates=e->map->nr_candidates;
    for(size_t i=0;i<q.nr_candidates;++i)q.candidate_ram[i]=e->map->candidates[i];
    const struct boot_range protected[]={e->live.stack,e->live.handoff,
        e->live.source,e->workspace};
    q.nr_protected=sizeof protected/sizeof protected[0];
    for(size_t i=0;i<q.nr_protected;++i)q.protected_ranges[i]=protected[i];
    const uint64_t *old_pml4=(const uint64_t*)(uintptr_t)p->old_root;
    struct igp785_tables *t=(struct igp785_tables*)(void*)frames[IGP785_NEW_PML4];
    struct igp785_layout plan={0};
    if(igp785_prepare_tables(&q,old_pml4,t,&plan)!=IGP785_OK ||
       !igp785_audit_tables(&q,old_pml4,t,&plan) ||
       plan.new_root_pa!=q.pages[IGP785_NEW_PML4] ||
       plan.hardware_ist_proven || plan.activation_allowed)
        fail("guard-structural-audit");
    /* Cross-check every actually populated PML4/PDPT/PD/PT entry against
     * the identities that were proved under inherited CR3. */
    if(t->pml4[128]!=(q.pages[IGP785_NEW_PDPT]|3u) ||
       t->pdpt[0]!=(q.pages[IGP785_NEW_PD]|3u) ||
       t->pd[0]!=(q.pages[IGP785_NEW_PT]|3u) ||
       t->pt[3]!=(q.pages[IGP785_IST1]|IGP785_STACK_PTE))
        fail("guard-physical-audit");
    /* GDT: old legacy selectors, plus one fresh available 16-byte TSS.
     * RSP0 stays inside a distinct image frame; IST1 uses the guarded VA. */
    struct desc_ptr old_gdtr={0},old_idtr={0};
    __asm__ volatile("sgdt %0;sidt %1":"=m"(old_gdtr),"=m"(old_idtr)::"memory");
    const uint16_t cs=current_cs();
    if(!cs || (cs&7u) || !old_gdtr.base || old_gdtr.base>=LONG_MODE_LIMIT ||
       !old_idtr.base || old_idtr.base>=LONG_MODE_LIMIT ||
       old_idtr.limit!=4095u || old_gdtr.limit>=PAGE-16u ||
       (old_gdtr.limit&7u)!=7u)
        fail("descriptor-context");
    const struct tis782_plan stage={
        q.pages[IGP785_RSP0]+PAGE-16u,IGP785_IST_ALIAS+PAGE-16u,true,false};
    unsigned char *tss=frames[IGP785_TSS],*gdt=frames[IGP785_GDT];
    if(!tis782_encode(&stage,tss,TIS782_TSS_SIZE))fail("tss-encode");
    struct tgd783_result desc={0};
    if(tgd783_build((const uint8_t*)(uintptr_t)old_gdtr.base,
         (size_t)old_gdtr.limit+1u,cs,0,q.pages[IGP785_TSS],tss,
         TIS782_TSS_SIZE,&stage,gdt,PAGE,&desc)!=TGD783_OK ||
       !tgd783_verify(gdt,PAGE,tss,TIS782_TSS_SIZE,&desc))
        fail("tss-descriptor");
    /* IDT: copy the proven existing 256 gates, replace precisely vector 3.
     * The #BP handler never IRETs and never calls firmware. */
    unsigned char *idt=frames[IGP785_NEW_IDT];
    const unsigned char *inherited=(const unsigned char*)(uintptr_t)old_idtr.base;
    for(size_t i=0;i<PAGE;++i)idt[i]=inherited[i];
    struct idt_gate_786 *gate=(struct idt_gate_786*)(void*)(idt+3u*16u);
    uintptr_t handler=(uintptr_t)&s786_isr3;
    *gate=(struct idt_gate_786){(uint16_t)handler,cs,1u,0x8eu,
        (uint16_t)(handler>>16u),(uint32_t)(handler>>32u),0};
    if(gate->ist!=1u || gate->selector!=cs ||
       read64(tss+36u)!=stage.ist1_top ||
       read64(tss+4u)!=stage.rsp0_top)
        fail("ist1-idt-gate");
    attested=(struct state786){p->old_root,plan.new_root_pa,
        e->live.stack.start,e->live.stack.end,0,
        q.pages[IGP785_NEW_IDT],q.pages[IGP785_GDT],cs,desc.tss_selector,IST_MARKER};
    struct desc_ptr active_gdt={desc.new_gdt_limit,q.pages[IGP785_GDT]};
    struct desc_ptr active_idt={4095u,q.pages[IGP785_NEW_IDT]};
    field("STYLEOS S786 READY old=",attested.old_cr3);
    field(" new=",attested.new_cr3);
    field(" alias=",IGP785_IST_ALIAS);
    field(" guard-lo=",IGP785_IST_ALIAS-PAGE);
    field(" guard-hi=",IGP785_IST_ALIAS+PAGE);
    field(" frame=",q.pages[IGP785_IST1]);
    field(" cs=",cs);field(" selector=",desc.tss_selector);
    field(" idt=",q.pages[IGP785_NEW_IDT]);
    arch_puts(" irq=0 nxe=1 own=9 tables=inactive-preflight\n");
    /* Terminal one-way transition. All inherited PML4 slots except 128 are
     * byte-for-byte unchanged; no UEFI return or TR restoration is legal. */
    __asm__ volatile("cli;mov %0,%%cr3"::"r"(plan.new_root_pa):"memory");
    if(cr3()!=plan.new_root_pa)fail("cr3-readback");
    __asm__ volatile("lgdt %0"::"m"(active_gdt):"memory");
    __asm__ volatile("ltr %0"::"rm"(desc.tss_selector):"memory");
    if(current_tr()!=desc.tss_selector || gdt[desc.tss_selector+5u]!=0x8bu)
        fail("ltr-or-busy");
    __asm__ volatile("lidt %0"::"m"(active_idt):"memory");
    *(volatile uint64_t*)(uintptr_t)(IGP785_IST_ALIAS+0x60u)=IST_MARKER;
    if(*(volatile uint64_t*)(uintptr_t)(q.pages[IGP785_IST1]+0x60u)!=IST_MARKER)
        fail("alias-physical-store");
    /* This instruction MUST transfer control through IDT[3] IST1.
     * Any unexpected return triggers UD2, which cannot produce PASS. */
    s786_int3();
}
#endif
