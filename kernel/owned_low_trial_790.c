/* StyleOS 790: terminal, owned slot-0 4KiB hierarchy with PE WX separation.
 * Nested hardware #DF is repeated with NO inherited low-level page tables.
 * No firmware return, no production Desktop, no stack exhaustion claim. */
#include <styleos/owned_low_trial_790.h>
#include <styleos/ist_guard_plan_785.h>
#include <styleos/owned_low_790.h>
#include <styleos/tss_descriptor_783.h>
#include <styleos/tss_ist_stage_782.h>
#include <styleos/paging.h>
#include <styleos/arch.h>
#ifdef STYLEOS_TEST_APIC_ROOT_842
#include <styleos/apic_pe_842.h>
_Noreturn void irq842_terminal(const struct boot_info *,uint64_t,uint64_t);
void irq807_unexpected(void);
static struct s842_root apic842;
static struct s842_cache cache842;
#ifdef STYLEOS_TEST_DESKTOP_PREFLIGHT_850
#include <styleos/desktop_root_850.h>
_Noreturn void s850_preflight(const struct s842_root *,const struct s842_cache *,struct pg_experiment *,const struct s846_guard *,uint64_t,uint64_t,uint64_t,uint64_t
#ifdef STYLEOS_TEST_DESKTOP_SWITCH_851
,uint16_t,uint16_t
#endif
);
#endif
#ifdef STYLEOS_TEST_USER_ROOTS_846
bool s846_bind(const struct s842_root *,const struct s842_cache *,struct pg_experiment *,const struct igp785_tables *,uint64_t,uint64_t,uint64_t,uint64_t);
#endif
#ifdef STYLEOS_TEST_TIMER_SCHEDULER_843
bool s843_admit(const struct s842_root *,struct pg_experiment *,uint64_t,uint64_t);
#endif
#endif

#ifdef STYLEOS_TEST_OWNED_LOW_790
#define PAGE 4096u
_Alignas(PAGE) static uint64_t owned_low_tables[PG_TABLE_PAGES][512];
static struct s790_low_result owned_low;
#define LONG_MODE_LIMIT (UINT64_C(1)<<47)
#define IST_MARKER UINT64_C(0x78815a0cafe12345)
#define IST2_ALIAS (IGP785_IST_ALIAS+3u*PAGE)
#define IST2_FRAME IGP785_FRAMES
#define IST3_ALIAS (IGP785_IST_ALIAS+6u*PAGE)
#define IST3_FRAME (IGP785_FRAMES+1u)
#define GUARD_ADDRESS (IGP785_IST_ALIAS-PAGE)

/* Eleven TSS/IST image-owned frames; plus the detached low-table pages. */
_Alignas(PAGE) static unsigned char frames[IGP785_FRAMES+2u][PAGE];
struct desc_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));
struct idt_gate_790 {
    uint16_t off0, selector;
    uint8_t ist, attributes;
    uint16_t off1;
    uint32_t off2, reserved;
} __attribute__((packed));
_Static_assert(sizeof(struct idt_gate_790)==16u,"x86 IDT gate width");
_Static_assert(sizeof(struct desc_ptr)==10u,"descriptor pointer width");
_Static_assert(sizeof(struct igp785_tables)==4u*PAGE,"exact table pages");

struct state790 {
    uint64_t old_cr3, new_cr3;
    uint64_t boot_stack_start, boot_stack_end;
    uint64_t old_rsp_before;
    uint64_t idt_base,gdt_base;
    uint16_t cs, tr_selector;
    uint64_t ready;
    uint64_t inherited_low_entry;
    uint64_t bp_verified;
    uint64_t image_start,image_end;
};
static struct state790 attested;

static uint64_t cr3(void) { uint64_t n; __asm__ volatile("mov %%cr3,%0":"=r"(n));return n; }
static uint64_t cr0(void) { uint64_t n; __asm__ volatile("mov %%cr0,%0":"=r"(n));return n; }
static uint64_t cr4(void) { uint64_t n; __asm__ volatile("mov %%cr4,%0":"=r"(n));return n; }
static uint64_t rflags(void) { uint64_t n; __asm__ volatile("pushfq;popq %0":"=r"(n));return n; }
static uint16_t current_cs(void) { uint16_t n; __asm__ volatile("mov %%cs,%0":"=r"(n));return n; }
static uint16_t current_tr(void) { uint16_t n; __asm__ volatile("str %0":"=r"(n));return n; }
#ifdef STYLEOS_TEST_APIC_ROOT_842
static uint64_t msr842(uint32_t n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return ((uint64_t)d<<32)|a;}
static void cp842(uint32_t leaf,uint32_t *a,uint32_t *d){
 uint32_t b,c;__asm__ volatile("cpuid":"=a"(*a),"=b"(b),"=c"(c),"=d"(*d):"a"(leaf),"c"(0));(void)b;(void)c;
}
static bool snap842(struct s842_cache *s){
 uint32_t a,d;cp842(1,&a,&d);if((d&0x1120u)!=0x1120u||!(d&(1u<<16)))return false;
 cp842(0x80000000u,&a,&d);if(a<0x80000008u)return false;
 cp842(0x80000008u,&a,&d);s->bits=(uint8_t)a;
 if(s->bits<36||s->bits>52||cr0()&(UINT64_C(3)<<29))return false;
 s->pat=msr842(0x277);s->cap=msr842(0xfe);s->def=msr842(0x2ff);s->count=(uint8_t)s->cap;
 if(s->count>CACHE800_MAX_VAR)return false;
 for(unsigned i=0;i<s->count;i++){s->pairs[i].base=msr842(0x200+2*i);s->pairs[i].mask=msr842(0x201+2*i);}
 return true;
}
static bool cache_unchanged842(const struct s842_cache *s){
 if(msr842(0x277)!=s->pat||msr842(0xfe)!=s->cap||msr842(0x2ff)!=s->def)return false;
 for(unsigned i=0;i<s->count;i++)if(msr842(0x200+2*i)!=s->pairs[i].base||msr842(0x201+2*i)!=s->pairs[i].mask)return false;
 return true;
}
#endif
static uint64_t efer(void) { uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000080u));return ((uint64_t)hi<<32u)|lo; }
static void field(const char *s,uint64_t v) { arch_puts(s);paging_hex(v); }
__attribute__((noreturn)) static void stop_vm(unsigned status) {
#ifdef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
    (void)status;
    __asm__ volatile("cli":::"memory");
#else
    __asm__ volatile("cli;outl %0,%1"::"a"(status),"Nd"((uint16_t)0xf4):"memory");
#endif
    arch_halt();
}
__attribute__((noreturn)) static void fail(const char *why) {
    arch_puts("STYLEOS S790 FAIL ");arch_puts(why);arch_putc('\n');stop_vm(0x11u);
}
extern void s790_after_int3(void);
__attribute__((noreturn,noinline,used,naked)) static void s790_int3(void) {
    __asm__ volatile("int3\n.globl s790_after_int3\ns790_after_int3:\nud2\n");
}
/* Capture the ORIGINAL interrupt RSP before the Microsoft x64 call frame.
 * Do not use a returning handler: no UEFI, IRET, or test CPU reset. */
__attribute__((noreturn,noinline,used)) void s790_on_int3(uint64_t cpu_rsp);
__attribute__((noreturn,noinline,used)) void s790_on_df(uint64_t cpu_rsp);
__attribute__((noreturn,noinline,used,naked)) static void s790_isr3(void) {
    __asm__ volatile("cli\nmovq %rsp,%rcx\nandq $-16,%rsp\nsubq $32,%rsp\ncall s790_on_int3\nud2\n");
}
__attribute__((noreturn,noinline,used,naked)) static void s790_isr_df(void) {
    __asm__ volatile("cli\nmovq %rsp,%rcx\nandq $-16,%rsp\nsubq $32,%rsp\ncall s790_on_df\nud2\n");
}
static uint64_t read64(const void *source) {
    const unsigned char *p=(const unsigned char*)source;
    uint64_t v=0;
    for(unsigned i=0;i<8;++i) v|=(uint64_t)p[i]<<(8u*i);
    return v;
}
/* #BP enters on IST1. A guard write triggers hardware #PF, but its gate
 * is marked not-present; CPU must deliver #DF on the independent IST3. */
extern void s790_guard_fault_ip(void);
__attribute__((noreturn,noinline,used)) void s790_on_int3(uint64_t cpu_rsp) {
    const uint64_t low=IGP785_IST_ALIAS, high=low+PAGE;
    if(attested.ready!=IST_MARKER || cr3()!=attested.new_cr3 ||
       current_tr()!=attested.tr_selector ||
       cpu_rsp<low || cpu_rsp>high-5u*8u || (cpu_rsp&7u))
        fail("bp-entry-context");
    const uint64_t *hardware=(const uint64_t*)(uintptr_t)cpu_rsp;
    if(hardware[0]!=(uint64_t)(uintptr_t)&s790_after_int3 ||
       hardware[1]!=attested.cs || !(hardware[2]&2u) ||
       hardware[3]<attested.boot_stack_start ||
       hardware[3]>=attested.boot_stack_end)
        fail("bp-hardware-frame");
    attested.bp_verified=1;
    field("STYLEOS S790 BP cr3=",cr3());
    field(" rsp=",cpu_rsp);field(" saved-rip=",hardware[0]);
    field(" expected-rip=",(uint64_t)(uintptr_t)&s790_after_int3);
    field(" tr=",current_tr());arch_putc('\n');
    /* Label is attached to the ACTUAL faulting MOV instruction. A mapped
     * page would allow the store and immediately go to failure below. */
    const uint64_t address=GUARD_ADDRESS, value=IST_MARKER;
    __asm__ volatile(".globl s790_guard_fault_ip\ns790_guard_fault_ip: movq %1,(%0)\n"
        ::"r"(address),"r"(value):"memory");
    fail("guard-write-did-not-page-fault");
}
__attribute__((noreturn,noinline,used)) void s790_on_df(uint64_t cpu_rsp) {
    const struct igp785_tables *t=(const struct igp785_tables*)(const void*)frames[IGP785_NEW_PML4];
    const uint64_t lo=IST3_ALIAS, hi=IST3_ALIAS+PAGE;
    struct desc_ptr gdtr={0},idtr={0};
    __asm__ volatile("sgdt %0;sidt %1":"=m"(gdtr),"=m"(idtr)::"memory");
    uint64_t cr2=0;
    __asm__ volatile("mov %%cr2,%0":"=r"(cr2));
    const struct idt_gate_790 *pf_gate=(const struct idt_gate_790*)(const void*)
        (frames[IGP785_NEW_IDT]+14u*16u);
    const struct idt_gate_790 *df_gate=(const struct idt_gate_790*)(const void*)
        (frames[IGP785_NEW_IDT]+8u*16u);
    const uint64_t ist3_pa=(uint64_t)(uintptr_t)frames[IST3_FRAME];
    const uint64_t ist3_top=IST3_ALIAS+PAGE-16u;
    if(attested.ready!=IST_MARKER || attested.bp_verified!=1 ||
       cr3()!=attested.new_cr3 || current_tr()!=attested.tr_selector ||
       !s790_audit_owned_low(&owned_low,
         (uint64_t)(uintptr_t)&s790_on_df,(uint64_t)(uintptr_t)&attested,
         attested.old_rsp_before) ||
       ((t->pml4[0]^owned_low.new_low_slot)&~UINT64_C(0x20)) ||
       ((t->pml4[0]&UINT64_C(0x20)) <
          (owned_low.new_low_slot&UINT64_C(0x20))) ||
       ((t->pml4[0]^attested.inherited_low_entry)&~UINT64_C(0x20))==0 ||
       (t->pml4[128]&UINT64_C(0x20))==0 ||
       current_cs()!=attested.cs || (rflags()&(1u<<9u)) ||
       idtr.base!=attested.idt_base || idtr.limit!=4095u ||
       gdtr.base!=attested.gdt_base ||
       (cr0() & (UINT64_C(1)<<16u))==0 ||
       (efer() & (UINT64_C(1)<<11u))==0 ||
       cpu_rsp<lo || cpu_rsp>hi-6u*8u || (cpu_rsp&7u) ||
       cr2!=GUARD_ADDRESS ||
       t->pt[2] || t->pt[4] || t->pt[5] || t->pt[7] || t->pt[8] || t->pt[10] ||
       (t->pt[3]&~UINT64_C(0x60))!=
         ((uint64_t)(uintptr_t)frames[IGP785_IST1]|IGP785_STACK_PTE) ||
       (t->pt[6]&~UINT64_C(0x60))!=
         ((uint64_t)(uintptr_t)frames[IST2_FRAME]|IGP785_STACK_PTE) ||
       (t->pt[9]&~UINT64_C(0x60))!=(ist3_pa|IGP785_STACK_PTE) ||
       (t->pt[3]&0x60)!=0x60 || (t->pt[9]&0x60)!=0x60 ||
       *(volatile uint64_t*)(uintptr_t)(IGP785_IST_ALIAS+0x60)!=IST_MARKER ||
       (pf_gate->attributes&0x80u) || pf_gate->ist!=2u ||
       pf_gate->selector!=attested.cs ||
       df_gate->attributes!=0x8eu || df_gate->ist!=3u ||
       df_gate->selector!=attested.cs ||
       read64(frames[IGP785_TSS]+52u)!=ist3_top ||
       frames[IGP785_GDT][attested.tr_selector+5u]!=0x8bu)
        fail("df-active-hardware-or-ist3");
    const uint64_t *hw=(const uint64_t*)(uintptr_t)cpu_rsp;
    const uint64_t error=hw[0],rip=hw[1],cs=hw[2],flags=hw[3],old_rsp=hw[4];
    /* Intel double fault carries error code zero. A #DF is distinct from
     * ordinary #PF, which must not reach a handler for this experiment. */
    if(error!=0 || rip!=(uint64_t)(uintptr_t)&s790_guard_fault_ip ||
       cs!=attested.cs || !(flags&2u) ||
       old_rsp<IGP785_IST_ALIAS || old_rsp>=IGP785_IST_ALIAS+PAGE)
        fail("df-hardware-frame-or-error-code");
    field("STYLEOS S790 DF cr2=",cr2);
    field(" err=",error);field(" rsp=",cpu_rsp);
    field(" saved-rip=",rip);
    field(" expected-rip=",(uint64_t)(uintptr_t)&s790_guard_fault_ip);
    field(" old-rsp=",old_rsp);field(" cs=",cs);
    field(" tr=",current_tr());
    field(" leaf1=",t->pt[3]);field(" leaf2=",t->pt[6]);
    field(" leaf3=",t->pt[9]);
    field(" root0=",t->pml4[0]);
    field(" root128=",t->pml4[128]);
    field(" oldroot0=",attested.inherited_low_entry);
    field(" low-table=",owned_low.built.root);
    field(" low-used=",owned_low.built.used);
    field(" code-rx=",owned_low.code_pages);
    field(" data-rw-nx=",owned_low.writable_pages);
    field(" ro-nx=",owned_low.readonly_pages);
    field(" stack-rw-nx=",owned_low.stack_pages);
    arch_puts(" inherited-root-slots=0 root-present-slots=2 pf-gate-present=0 df-ist3=1 hardware-df=1\n");
    arch_puts("STYLEOS S790 TERMINAL qemu-exit=0x10 firmware-return=0 pf-handler-entered=0 df-hardware=1 pml4-pruned=1 independent-low-tables=1 pe-wx-page-policy=1 stack-overflow-recovery=0\n");
    stop_vm(0x10u);
}

__attribute__((noreturn)) void s790_terminal_trial(struct pg_experiment *p) {
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
    /* Independently attest both extra image-owned IST frames. */
    const uint64_t ist2_pa=(uint64_t)(uintptr_t)frames[IST2_FRAME];
    const uint64_t ist3_pa=(uint64_t)(uintptr_t)frames[IST3_FRAME];
    if(arch_paging_attest_image_writable(p,ist2_pa,PAGE)!=PG_OK ||
       arch_paging_attest_image_writable(p,ist3_pa,PAGE)!=PG_OK ||
       ist2_pa==p->old_root || ist3_pa==p->old_root || ist2_pa==ist3_pa)
        fail("extra-ist-physical-frame-attestation");
    for(size_t i=0;i<IGP785_FRAMES;++i)
        if(ist2_pa==q.pages[i] || ist3_pa==q.pages[i])fail("ist-frame-collision");
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
    /* Audit immutable 785 plan first; then add IST2 and IST3, each with
     * missing adjacent PTEs. The active #DF handler uses IST3, not IST2. */
    for(size_t i=5;i<=10;++i)if(t->pt[i])fail("extra-pt-not-empty");
    t->pt[6]=ist2_pa|IGP785_STACK_PTE;
    t->pt[9]=ist3_pa|IGP785_STACK_PTE;
    if(t->pt[2] || t->pt[4] || t->pt[5] || t->pt[7] || t->pt[8] || t->pt[10])
        fail("ist-guard-layout");
    /* Preflight alias entries before replacing the inherited low slot. */
    if(t->pml4[128]!=(q.pages[IGP785_NEW_PDPT]|3u) ||
       t->pdpt[0]!=(q.pages[IGP785_NEW_PD]|3u) ||
       t->pd[0]!=(q.pages[IGP785_NEW_PT]|3u) ||
       t->pt[3]!=(q.pages[IGP785_IST1]|IGP785_STACK_PTE))
        fail("guard-physical-audit");
    /* Slot zero is replaced with our image-owned PDPT/PD/PT hierarchy.
     * Only already-admitted identity image/boot-stack pages are mapped.
     * PE section roles must not mix writable and executable in one page. */
    uint64_t inherited_rsp=0;
    __asm__ volatile("mov %%rsp,%0":"=r"(inherited_rsp));
    if(arch_paging_attest_image_writable(p,(uint64_t)(uintptr_t)owned_low_tables,
             sizeof owned_low_tables)!=PG_OK)
        fail("owned-low-tables-physical-attestation");
    const struct s790_low_request low_request={
        .image=e->live.image,.boot_stack=e->live.stack,
        .loaded_image=(const void*)(uintptr_t)e->live.image.start,
        .owned_tables=owned_low_tables,
        .owned_tables_pa=(uint64_t)(uintptr_t)owned_low_tables,
        .required_exec=(uint64_t)(uintptr_t)&s790_on_df,
        .required_write=(uint64_t)(uintptr_t)&attested,
        .required_rsp=inherited_rsp};
    if(!s790_prepare_owned_low(&low_request,&owned_low) ||
       !s790_audit_owned_low(&owned_low,low_request.required_exec,
             low_request.required_write,low_request.required_rsp))
        fail("cannot-stage-owned-pe-low-hierarchy");
#ifdef STYLEOS_TEST_APIC_ROOT_842
    const struct boot_info *boot842=(const struct boot_info *)(uintptr_t)e->live.handoff.start;
    struct acpi806_topology topo842={0};
#ifdef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
    /* The captured MADT is the same validated firmware snapshot consumed
     * by the APIC-root builder. Explain unsupported topology before it
     * can reach that builder; never relax its single-LAPIC contract. */
    if(!acpi806_copy_capture(&topo842))fail("s842-acpi-capture");
    if(topo842.enabled_lapics!=1){
        arch_puts("STYLEOS S790 CPU-SCOPE");
        field(" enabled=",topo842.enabled_lapics);
        arch_putc('\n');
        fail("863-platform-cpu");
    }
    if(!snap842(&cache842)||
#else
    if(!acpi806_copy_capture(&topo842)||!snap842(&cache842)||
#endif
       !s842_build_pe_efi(&low_request,e,boot842,&cache842,&topo842,msr842(0x1b),&apic842)||
       !s842_audit(&apic842,&cache842)||!cache_unchanged842(&cache842))fail("s842-apic-root-admission");
#endif
#ifdef STYLEOS_TEST_USER_ROOTS_846
    if(!s846_bind(&apic842,&cache842,p,t,plan.new_root_pa,q.pages[IGP785_IST1],ist2_pa,ist3_pa))fail("s846-storage");
#endif
#ifdef STYLEOS_TEST_TIMER_SCHEDULER_843
    if(!s843_admit(&apic842,p,plan.new_root_pa,IGP785_IST_ALIAS))fail("s843-task-admission");
#endif
    for(size_t i=0;i<512u;++i)if(i!=128u)t->pml4[i]=0;
#ifdef STYLEOS_TEST_APIC_ROOT_842
    t->pml4[0]=apic842.plan.tables[0][0];
#else
    t->pml4[0]=owned_low.new_low_slot;
#endif
    if((t->pml4[0]&UINT64_C(0x000ffffffffff000))==
           (old_pml4[0]&UINT64_C(0x000ffffffffff000)) ||
       t->pml4[128]!=(q.pages[IGP785_NEW_PDPT]|3u))
        fail("low-slot-not-detached");
    /* GDT: old legacy selectors, plus one fresh available 16-byte TSS.
     * RSP0 stays inside a distinct image frame; IST1 uses the guarded VA. */
    struct desc_ptr old_gdtr={0},old_idtr={0};
    __asm__ volatile("sgdt %0;sidt %1":"=m"(old_gdtr),"=m"(old_idtr)::"memory");
    const uint16_t cs=current_cs();
    if(!cs || (cs&7u) || !old_gdtr.base || old_gdtr.base>=LONG_MODE_LIMIT ||
       !old_idtr.base || old_idtr.base>=LONG_MODE_LIMIT ||
       old_idtr.limit!=4095u ||
       old_gdtr.base>=(UINT64_C(1)<<39u) || old_idtr.base>=(UINT64_C(1)<<39u) ||
       old_gdtr.limit>=PAGE-16u ||
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
    /* Append IST2 and IST3 after the audited 783 descriptor preparation. */
    const uint64_t ist2_top=IST2_ALIAS+PAGE-16u;
    const uint64_t ist3_top=IST3_ALIAS+PAGE-16u;
    for(unsigned i=0;i<8u;++i){
        tss[44u+i]=(uint8_t)(ist2_top>>(8u*i));
        tss[52u+i]=(uint8_t)(ist3_top>>(8u*i));
    }
    if(read64(tss+44u)!=ist2_top || read64(tss+52u)!=ist3_top)
        fail("tss-extra-ist-stage");
    /* Install #BP IST1 and #DF IST3. Vector #PF is intentionally NOT
     * PRESENT to force the nested exception classification. The existing
     * firmware IDT gates for other vectors are otherwise inherited. */
    unsigned char *idt=frames[IGP785_NEW_IDT];
    const unsigned char *inherited=(const unsigned char*)(uintptr_t)old_idtr.base;
    for(size_t i=0;i<PAGE;++i)idt[i]=inherited[i];
    struct idt_gate_790 *gate=(struct idt_gate_790*)(void*)(idt+3u*16u);
    uintptr_t handler=(uintptr_t)&s790_isr3;
    *gate=(struct idt_gate_790){(uint16_t)handler,cs,1u,0x8eu,
        (uint16_t)(handler>>16u),(uint32_t)(handler>>32u),0};
    struct idt_gate_790 *pf_gate=(struct idt_gate_790*)(void*)(idt+14u*16u);
    uintptr_t pf_handler=(uintptr_t)&s790_isr_df;
    *pf_gate=(struct idt_gate_790){(uint16_t)pf_handler,cs,2u,0x0eu,
        (uint16_t)(pf_handler>>16u),(uint32_t)(pf_handler>>32u),0};
    struct idt_gate_790 *df_gate=(struct idt_gate_790*)(void*)(idt+8u*16u);
    uintptr_t df_handler=(uintptr_t)&s790_isr_df;
    *df_gate=(struct idt_gate_790){(uint16_t)df_handler,cs,3u,0x8eu,
        (uint16_t)(df_handler>>16u),(uint32_t)(df_handler>>32u),0};
    if(pf_gate->attributes!=0x0eu || pf_gate->ist!=2u ||
       pf_gate->selector!=cs || df_gate->attributes!=0x8eu ||
       df_gate->ist!=3u || df_gate->selector!=cs ||
       gate->ist!=1u || gate->selector!=cs ||
       read64(tss+36u)!=stage.ist1_top || read64(tss+4u)!=stage.rsp0_top ||
       read64(tss+44u)!=ist2_top || read64(tss+52u)!=ist3_top)
        fail("df-idt-gate");
#ifdef STYLEOS_TEST_APIC_ROOT_842
    /* No inherited handler may be reached under the detached root. Use
     * guarded IST3 for unexpected vectors, including transition faults. */
    uintptr_t unexpected=(uintptr_t)&irq807_unexpected;
    for(unsigned i=0;i<256;i++)((struct idt_gate_790*)(void*)idt)[i]=
       (struct idt_gate_790){(uint16_t)unexpected,cs,3u,0x8eu,
       (uint16_t)(unexpected>>16u),(uint32_t)(unexpected>>32u),0};
    field("STYLEOS S842 READY old=",p->old_root);field(" new=",plan.new_root_pa);
    field(" low-table=",apic842.built.root);field(" low-used=",apic842.built.used);
    field(" lapic=",apic842.lapic);field(" ioapic=",apic842.ioapic);
    field(" uc-index=",apic842.uc_index);field(" pat=",cache842.pat);
    field(" ist=",IGP785_IST_ALIAS);arch_putc('\n');
#endif
#ifdef STYLEOS_TEST_DESKTOP_PREFLIGHT_850
    const struct s846_guard g850={{q.pages[IGP785_IST1],ist2_pa,ist3_pa}};
    s850_preflight(&apic842,&cache842,p,&g850,q.pages[IGP785_GDT],q.pages[IGP785_NEW_IDT],q.pages[IGP785_TSS],q.pages[IGP785_RSP0]
#ifdef STYLEOS_TEST_DESKTOP_SWITCH_851
,desc.new_gdt_limit,desc.tss_selector
#endif
);
#endif
    attested=(struct state790){p->old_root,plan.new_root_pa,
        e->live.stack.start,e->live.stack.end,inherited_rsp,
        q.pages[IGP785_NEW_IDT],q.pages[IGP785_GDT],cs,desc.tss_selector,IST_MARKER,
        old_pml4[0],0,e->live.image.start,e->live.image.end};
    struct desc_ptr active_gdt={desc.new_gdt_limit,q.pages[IGP785_GDT]};
    struct desc_ptr active_idt={4095u,q.pages[IGP785_NEW_IDT]};
    field("STYLEOS S790 READY old=",attested.old_cr3);
    field(" new=",attested.new_cr3);
    field(" alias=",IGP785_IST_ALIAS);
    field(" guard-lo=",IGP785_IST_ALIAS-PAGE);
    field(" guard-hi=",IGP785_IST_ALIAS+PAGE);
    field(" frame=",q.pages[IGP785_IST1]);
    field(" cs=",cs);field(" selector=",desc.tss_selector);
    field(" idt=",q.pages[IGP785_NEW_IDT]);
    field(" ist2=",IST2_ALIAS);field(" guard=",GUARD_ADDRESS);
    field(" frame2=",ist2_pa);field(" ist3=",IST3_ALIAS);
    field(" frame3=",ist3_pa);
    field(" root0=",t->pml4[0]);
    field(" root128=",t->pml4[128]);
    field(" oldroot0=",old_pml4[0]);
    field(" low-table=",owned_low.built.root);
    field(" low-used=",owned_low.built.used);
    field(" code-rx=",owned_low.code_pages);
    field(" data-rw-nx=",owned_low.writable_pages);
    field(" ro-nx=",owned_low.readonly_pages);
    field(" stack-rw-nx=",owned_low.stack_pages);
    arch_puts(" inherited-root-slots=0 root-present-slots=2 irq=0 nxe=1 own=11 pf-gate-present=0 tables=inactive-preflight\n");
    /* No inherited identity PDPT/PD/PT after this point. Disable PGE
     * before MOV CR3 so stale global UEFI TLB translations cannot bypass
     * new read-only and NX leaf permissions. Never return to firmware. */
    const uint64_t cr4_no_global=cr4()&~(UINT64_C(1)<<7u);
    if((cr4_no_global&(UINT64_C(1)<<17u))!=0)
        fail("pcid-not-supported");
    __asm__ volatile("cli;mov %0,%%cr4;mov %1,%%cr3"::
        "r"(cr4_no_global),"r"(plan.new_root_pa):"memory");
    if(cr3()!=plan.new_root_pa)fail("cr3-readback");
    __asm__ volatile("lgdt %0"::"m"(active_gdt):"memory");
    __asm__ volatile("ltr %0"::"rm"(desc.tss_selector):"memory");
    if(current_tr()!=desc.tss_selector || gdt[desc.tss_selector+5u]!=0x8bu)
        fail("ltr-or-busy");
    __asm__ volatile("lidt %0"::"m"(active_idt):"memory");
    *(volatile uint64_t*)(uintptr_t)(IGP785_IST_ALIAS+0x60u)=IST_MARKER;
    if(*(volatile uint64_t*)(uintptr_t)(q.pages[IGP785_IST1]+0x60u)!=IST_MARKER)
        fail("alias-physical-store");
    /* INT3 → IST1 → absent guard #PF → absent #PF gate → #DF IST3.
     * Any return instead triggers UD2; no emulated PASS is possible. */
#ifdef STYLEOS_TEST_APIC_ROOT_842
    if(!s842_audit(&apic842,&cache842)||!cache_unchanged842(&cache842)||
       ((t->pml4[0]^apic842.plan.tables[0][0])&~UINT64_C(0x20))||
       (t->pml4[128]&~UINT64_C(0x20))!=(q.pages[IGP785_NEW_PDPT]|3u))
        fail("s842-post-switch-audit");
    for(unsigned i=0;i<512;i++)if(i!=0&&i!=128&&t->pml4[i])fail("s842-root-slot");
    irq842_terminal(boot842,plan.new_root_pa,IGP785_IST_ALIAS);
#else
    s790_int3();
#endif
}
#endif
