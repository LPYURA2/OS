#include <styleos/tss_ltr_trial_784.h>
#include <styleos/descriptor_state_781.h>

#define LOW_VA (UINT64_C(1) << 47u)

enum tlt784_status tlt784_preflight(const struct tlt784_context *c,
    const struct tgd783_result *r, const uint8_t *gdt, size_t gdt_bytes,
    const uint8_t *tss, size_t tss_bytes,
    const uint8_t *idt, size_t idt_bytes)
{
    if (!c || !r || !gdt || !tss || !idt || !c->old_cr3 ||
        !c->gdt_base || !c->idt_base || c->gdt_base >= LOW_VA ||
        c->idt_base >= LOW_VA || c->cs == 0 || (c->cs & 7u) ||
        c->tr != 0 || !c->interrupts_disabled || !c->idt_mapped ||
        !c->pages_attested || c->idt_limit != 4095u ||
        idt_bytes != 4096u || c->old_limit >= 4096u ||
        (c->old_limit & 7u) != 7u ||
        (uint32_t)c->old_limit + 16u >= 4096u) return TLT784_BAD_CONTEXT;
    if (gdt_bytes != TGD783_GDT_BYTES ||
        r->tss_selector != (uint16_t)(c->old_limit + 1u) ||
        r->new_gdt_limit != (uint16_t)(c->old_limit + 16u) ||
        !tgd783_verify(gdt,gdt_bytes,tss,tss_bytes,r))
        return TLT784_BAD_DESCRIPTOR;
    struct dsi781_report report = {0};
    if (!dsi781_audit_idt(idt,idt_bytes,c->cs,0,r->new_gdt_limit,&report) ||
        report.gates != 256u || report.ist_gates != 0 ||
        report.ist_index_mask || report.activation_permitted)
        return TLT784_BAD_HANDLERS;
    return TLT784_OK;
}

#ifdef STYLEOS_TEST_TSS_LTR_784
#include <styleos/arch.h>
#include <styleos/paging.h>

struct x86_descriptor_ptr { uint16_t limit; uint64_t base; }
    __attribute__((packed));
_Static_assert(sizeof(struct x86_descriptor_ptr) == 10u, "x86 descriptor pointer");

static uint64_t cr3_live(void) {
    uint64_t value;
    __asm__ volatile("mov %%cr3,%0" : "=r"(value));
    return value;
}
static uint64_t flags_live(void) {
    uint64_t value;
    __asm__ volatile("pushfq; popq %0" : "=r"(value));
    return value;
}
static uint16_t cs_live(void) {
    uint16_t value;
    __asm__ volatile("mov %%cs,%0" : "=r"(value));
    return value;
}
static uint16_t tr_live(void) {
    uint16_t value;
    __asm__ volatile("str %0" : "=r"(value));
    return value;
}
static void field(const char *name,uint64_t v) {
    arch_puts(name); paging_hex(v);
}
/* Both success AND failure leave the experiment. QEMU -device isa-debug-exit
 * is mandatory; without it this CPU halts forever. No ret/UEFI/LTR(0). */
__attribute__((noreturn,noinline))
static void terminate_vm(uint32_t value) {
    __asm__ volatile("cli; outl %0, %1" :: "a"(value), "Nd"((uint16_t)0xf4) : "memory");
    arch_halt();
}
__attribute__((noreturn,noinline))
static void terminal_failure(const char *reason) {
    arch_puts("STYLEOS TLT 784 FAIL "); arch_puts(reason); arch_putc('\n');
    terminate_vm(0x11u);  /* QEMU process exit 35, never PASS. */
}

_Noreturn void tlt784_terminal_trial(struct pg_experiment *p,
    const struct tgd783_result *r, uint8_t *gdt, const uint8_t *tss)
{
    struct x86_descriptor_ptr before={0},idtr={0};
    __asm__ volatile("cli; sgdt %0; sidt %1" : "=m"(before),"=m"(idtr) :: "memory");
    /* The previous 779/781 proof independently walked every IDT page,
     * active boot stack, GDT, and restored the original old CR3. Stage 782
     * attested all four image-owned GDT/TSS/stack pages under this same CR3.
     * Re-attest the two CPU-written/read descriptor pages immediately here. */
    if (!p || !r || !gdt || !tss ||
        arch_paging_attest_image_writable(p,(uint64_t)(uintptr_t)gdt,4096u)!=PG_OK ||
        arch_paging_attest_image_writable(p,(uint64_t)(uintptr_t)tss,4096u)!=PG_OK)
        terminal_failure("re-attestation");
    const struct tlt784_context context={
        p->old_root,before.base,idtr.base,before.limit,idtr.limit,
        cs_live(),tr_live(),!(flags_live()&(UINT64_C(1)<<9)),true,true
    };
    if (cr3_live()!=p->old_root ||
        tlt784_preflight(&context,r,gdt,4096u,tss,TIS782_TSS_SIZE,
                         (const uint8_t*)(uintptr_t)idtr.base,4096u)!=TLT784_OK)
        terminal_failure("preflight");
    struct x86_descriptor_ptr new_gdtr={r->new_gdt_limit,(uint64_t)(uintptr_t)gdt};
    field("STYLEOS TLT 784 READY cr3=",p->old_root);
    field(" cs=",context.cs);field(" tr-before=",context.tr);
    field(" old-gdt=",before.base);field(" new-gdt=",new_gdtr.base);
    field(" old-limit=",before.limit);field(" new-limit=",new_gdtr.limit);
    field(" idt-base=",idtr.base);
    field(" selector=",r->tss_selector);field(" tss=",r->tss_base);
    arch_puts(" idt=own-256-ist0 if=0 pages=attested guard=unproven\n");

    /* Terminal transition. The old CS remains valid because 783 preserved
     * the inherited code descriptor. 781 inspected every live IDT gate and
     * proved they all use the same CS and IST=0. Unexpected faults use the
     * existing terminal early exception handler, not a firmware callback.
     * Such a fault never produces the required QEMU success exit. */
    __asm__ volatile("lgdt %0" :: "m"(new_gdtr) : "memory");
    struct x86_descriptor_ptr active={0};
    __asm__ volatile("sgdt %0" : "=m"(active) :: "memory");
    if (active.base!=new_gdtr.base || active.limit!=new_gdtr.limit ||
        cs_live()!=context.cs || cr3_live()!=context.old_cr3)
        terminal_failure("sgdt-readback");
    __asm__ volatile("ltr %0" :: "rm"(r->tss_selector) : "memory");
    const uint16_t tr_after=tr_live();
    const uint8_t busy=gdt[r->tss_selector+5u];
    struct x86_descriptor_ptr idt_after={0};
    __asm__ volatile("sidt %0" : "=m"(idt_after) :: "memory");
    if (tr_after!=r->tss_selector || busy!=0x8bu ||
        cr3_live()!=context.old_cr3 || cs_live()!=context.cs ||
        idt_after.base!=idtr.base || idt_after.limit!=idtr.limit ||
        (flags_live()&(UINT64_C(1)<<9)))
        terminal_failure("str-or-busy-readback");
    field("STYLEOS TLT 784 VERIFIED cr3=",cr3_live());
    field(" cs=",cs_live());field(" str=",tr_after);
    field(" gdt=",active.base);field(" limit=",active.limit);
    field(" tss=",r->tss_base);field(" busy=",busy);
    field(" idt-base=",idt_after.base);
    arch_puts(" lgdt=1 ltr=1 ist-delivery=untested guard=unproven\n");
    arch_puts("STYLEOS TLT 784 TERMINAL qemu-exit=0x10 firmware-return=0 tr-restored=0\n");
    terminate_vm(0x10u);  /* QEMU process exit 33. */
}
#endif
