#include <styleos/frame_grant_proof.h>
#include <styleos/paging.h>
#include <styleos/arch.h>
#include <styleos/desktop_grant_bridge.h>
#include <styleos/kernel_closure_audit.h>
#include <styleos/tss_ist_stage_782.h>
#include <styleos/tss_descriptor_783.h>
#ifdef STYLEOS_TEST_TSS_LTR_784
#include <styleos/tss_ltr_trial_784.h>
#endif
#ifdef STYLEOS_TEST_IST_HW_786
#include <styleos/ist_hw_trial_786.h>
#endif
#ifdef STYLEOS_TEST_GUARD_PF_787
#include <styleos/guard_pf_trial_787.h>
#endif

/* Only present in the independent --test-frame-grants PE image.
 * Neither storage nor the six data pages are acquired from candidate RAM.
 * The arrays are never published to the ordinary desktop or to a PMM. */
_Alignas(DVM_PAGE_SIZE) struct dvm_storage styleos_grant_tables;
_Alignas(DVM_PAGE_SIZE) unsigned char
    styleos_grant_users[DVM_USER_SLOTS][DVM_USER_PAGES][DVM_PAGE_SIZE];
_Static_assert(sizeof styleos_grant_tables == 72u * 4096u,"72 page-table frames");
_Static_assert(sizeof styleos_grant_users == 6u * 4096u,"six private frames");
/* Four NEW non-overlapping pages, reserved by the experimental PE image.
 * This does not furnish hardware guard pages, active TSS or installed GDT. */
_Alignas(DVM_PAGE_SIZE) static unsigned char tis782_gdt[4096];
_Alignas(DVM_PAGE_SIZE) static unsigned char tis782_tss[4096];
_Alignas(DVM_PAGE_SIZE) static unsigned char tis782_rsp0[4096];
_Alignas(DVM_PAGE_SIZE) static unsigned char tis782_ist1[4096];
static struct dfo_owner styleos_grant_owner;
static bool grant_walk(void *ctx,uint64_t pa,uint64_t bytes)
{ return arch_paging_attest_image_writable((struct pg_experiment*)ctx,pa,bytes)==PG_OK; }
static bool refuse_walk(void *ctx,uint64_t pa,uint64_t bytes)
{ (void)ctx;(void)pa;(void)bytes;return false; }

static void put_field(const char *key,uint64_t value)
{ arch_puts(key); paging_hex(value); }
static void show(const char *name,uint64_t va,uint64_t bytes)
{
    arch_puts("STYLEOS GRANTS "); arch_puts(name);
    put_field(" va=",va); put_field(" pa=",va);
    put_field(" bytes=",bytes); arch_putc('\n');
}

void fg_experiment_run(struct pg_experiment *p)
{
    if (!p || !p->evidence || !p->old_root)
        paging_fail("frame-grants context");
    const struct boot_memory_evidence *e=p->evidence;
    const uint64_t table_va=(uint64_t)(uintptr_t)&styleos_grant_tables;
    struct fg_claims claims={0};
    claims.tables=table_va;
    for(unsigned i=0;i<DVM_USER_SLOTS;++i)
        for(unsigned j=0;j<DVM_USER_PAGES;++j)
            claims.user[i][j]=(uint64_t)(uintptr_t)&styleos_grant_users[i][j][0];
    if(fg_check_policy(e,&claims)!=FG_OK)
        paging_fail("frame-grants policy");
    if(arch_paging_attest_image_writable(p,table_va,sizeof styleos_grant_tables)!=PG_OK)
        paging_fail("frame-grants tables physical walk");
    show("TABLES",table_va,sizeof styleos_grant_tables);
    for(unsigned i=0;i<DVM_USER_SLOTS;++i)
        for(unsigned j=0;j<DVM_USER_PAGES;++j) {
            uint64_t va=claims.user[i][j];
            if(arch_paging_attest_image_writable(p,va,DVM_PAGE_SIZE)!=PG_OK)
                paging_fail("frame-grants user physical walk");
            /* Explicit independent per-frame line, validated against the PE
             * image's real BSS range by the strict Python QEMU parser. */
            const char *names[2][3]={{"USER_A_CODE","USER_A_STACK","USER_A_DATA"},
                                     {"USER_B_CODE","USER_B_STACK","USER_B_DATA"}};
            show(names[i][j],va,DVM_PAGE_SIZE);
        }
    /* Exercise negative policy cases without modifying the real claims.
     * These results are observations of the same map and checker. */
    struct fg_claims invalid=claims;
    invalid.user[1][0]=claims.user[0][0];
    if(fg_check_policy(e,&invalid)!=FG_CONFLICT)
        paging_fail("frame-grants alias refusal");
    invalid=claims;invalid.user[0][0]+=1;
    if(fg_check_policy(e,&invalid)!=FG_LAYOUT)
        paging_fail("frame-grants alignment refusal");
    invalid=claims;invalid.user[0][0]=e->live.stack.start & ~(DVM_PAGE_SIZE-1u);
    if(fg_check_policy(e,&invalid)!=FG_CONFLICT)
        paging_fail("frame-grants stack refusal");
    int checked_candidate=0;
    for(size_t n=0;n<e->map->nr_candidates;++n) {
        struct boot_range r=e->map->candidates[n];
        if(r.end-r.start<DVM_PAGE_SIZE || !r.start)continue;
        invalid=claims;invalid.user[0][0]=r.start;
        /* Outside the reserved image is an adequate refusal even if
         * the policy returns NOT_IMAGE before checking CANDIDATE. */
        if(fg_check_policy(e,&invalid)==FG_OK)
            paging_fail("frame-grants candidate accepted");
        checked_candidate=1;
        break;
    }
    if(!checked_candidate)
        paging_fail("frame-grants no candidate case");
    arch_puts("STYLEOS GRANTS REFUSALS alias=1 alignment=1 stack=1 candidate=1\n");
    arch_puts("STYLEOS GRANTS PASS pages=0x000000000000004e origin=image-owned no-pmm=1\n");
    arch_puts("STYLEOS GRANTS END\n");
    /* All grants were individually walked under the inherited CR3.
     * Build detached 762 roots ONLY after one intentional refusal. */
    struct dgb_result roots={0};
    if(dgb_prepare(&styleos_grant_owner,&styleos_grant_tables,e,&claims,
        e->live.image.start,refuse_walk,p,&roots)!=DGB_ATTEST ||
       styleos_grant_owner.ready || roots.safe_root)
        paging_fail("777 missing physical proof refusal");
    if(dgb_prepare(&styleos_grant_owner,&styleos_grant_tables,e,&claims,
        e->live.image.start,grant_walk,p,&roots)!=DGB_OK ||
       !styleos_grant_owner.ready || roots.proved_pages!=79u ||
       roots.safe_root!=table_va || roots.user_root[0]==roots.user_root[1] ||
       roots.user_root[0]==p->old_root || roots.user_root[1]==p->old_root)
        paging_fail("777 detached grant admission");
    arch_puts("STYLEOS DGB PASS pages=79 roots=3 active=0 no-pmm=1\n");
    /* Hardware test: one diagnostic supervisor page from 777 is NOT a
     * complete kernel context. Refuse before a CR3 switch; prove the new
     * audit cannot be bypassed merely by a ready detached root. */
    struct kca_manifest inventory={0};
    for(unsigned i=0;i<KCA_ROLE_COUNT;++i) {
        inventory.role[i].va=e->live.image.start+(uint64_t)i*DVM_PAGE_SIZE;
        inventory.role[i].pa=inventory.role[i].va;
    }
    enum kca_status audit=kca_audit_core(&styleos_grant_owner,
        &inventory,grant_walk,p);
    if(audit!=KCA_MISSING) paging_fail("778 incomplete root accepted");
    arch_puts("STYLEOS KCA 778 REFUSED missing-kernel-closure=1 active=0 no-cr3-switch=1\n");
    /* Real, read-only inventory of inherited UEFI translations. It is NOT
     * a proof that these are suitable entries for detached kernel roots. */
    if (arch_paging_inventory_inherited(p)!=PG_OK)
        paging_fail("779 inherited hardware inventory");
    /* 782: only stage a candidate context. No LTR, LGDT, IDT/IST gate
     * replacement, new CR3, or change to the firmware's active stack. */
    const struct tis782_claims tclaims={
        (uint64_t)(uintptr_t)tis782_gdt,
        (uint64_t)(uintptr_t)tis782_tss,
        (uint64_t)(uintptr_t)tis782_rsp0,
        (uint64_t)(uintptr_t)tis782_ist1};
    struct tis782_plan staged={0};
    if(tis782_prepare(e,&claims,&tclaims,refuse_walk,p,&staged)!=TIS782_UNATTESTED ||
       staged.staged || staged.ltr_permitted)
        paging_fail("782 missing old CR3 physical proof refused");
    struct tis782_claims tbad=tclaims;
    tbad.ist1=tclaims.rsp0;
    if(tis782_prepare(e,&claims,&tbad,grant_walk,p,&staged)!=TIS782_CONFLICT ||
       staged.staged) paging_fail("782 stack alias refused");
    tbad=tclaims; tbad.tss=claims.tables;
    if(tis782_prepare(e,&claims,&tbad,grant_walk,p,&staged)!=TIS782_CONFLICT ||
       staged.staged) paging_fail("782 old table alias refused");
    if(tis782_prepare(e,&claims,&tclaims,grant_walk,p,&staged)!=TIS782_OK ||
       !staged.staged || staged.ltr_permitted ||
       staged.rsp0_top != tclaims.rsp0+4096u-16u ||
       staged.ist1_top != tclaims.ist1+4096u-16u ||
       !tis782_encode(&staged,tis782_tss,TIS782_TSS_SIZE))
        paging_fail("782 page-backed TSS context staging");
    uint16_t tr_after=0;
    __asm__ volatile("str %0":"=r"(tr_after));
    if(tr_after!=0 || p->old_root==0)
        paging_fail("782 inherited TR unchanged");
    const char *tnames[]={"GDT","TSS","RSP0","IST1"};
    const uint64_t pages[]={tclaims.gdt,tclaims.tss,tclaims.rsp0,tclaims.ist1};
    for(unsigned j=0;j<4u;++j) {
        arch_puts("STYLEOS DSI 782 FRAME "); arch_puts(tnames[j]);
        put_field(" va=",pages[j]);put_field(" pa=",pages[j]);
        put_field(" bytes=",4096u);arch_putc('\n');
    }
    arch_puts("STYLEOS DSI 782 STAGED");
    put_field(" cr3=",p->old_root);
    put_field(" rsp0-top=",staged.rsp0_top);
    put_field(" ist1-top=",staged.ist1_top);
    arch_puts(" pages=4 attest=1 refuse=1 tr=0 ltr=0 guard=unproven active=0 no-cr3-switch=1\n");

    /* 783: inherited descriptors already passed the 781 audit. The four
     * PE-image pages passed 782 per-frame attestation. Construct a fresh,
     * NON-ACTIVE GDT; no firmware state or interrupt gates are changed. */
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) gdtr={0};
    uint16_t cs783=0,tr783=0;
    __asm__ volatile("sgdt %0" : "=m"(gdtr));
    __asm__ volatile("mov %%cs,%0" : "=r"(cs783));
    __asm__ volatile("str %0" : "=r"(tr783));
    if(!gdtr.base || gdtr.base>=(UINT64_C(1)<<47) ||
       gdtr.limit>=4096u || (gdtr.base&7u) || tr783 ||
       gdtr.limit+1u>4096u-16u || (gdtr.limit&7u)!=7u || !cs783)
        paging_fail("783 inherited GDT bounds");
    const uint8_t *inherited_gdt=(const uint8_t*)(uintptr_t)gdtr.base;
    struct tgd783_result descriptor={0};
    if(tgd783_build(inherited_gdt,(size_t)gdtr.limit+1u,cs783,1u,
         tclaims.tss,tis782_tss,TIS782_TSS_SIZE,&staged,
         tis782_gdt,sizeof tis782_gdt,&descriptor)!=TGD783_ARGUMENT ||
       descriptor.descriptor_valid)
        paging_fail("783 nonzero TR refusal");
    if(tgd783_build(inherited_gdt,(size_t)gdtr.limit+1u,cs783,tr783,
         tclaims.tss,tis782_tss,TIS782_TSS_SIZE,&staged,
         tis782_gdt,sizeof tis782_gdt,&descriptor)!=TGD783_OK ||
       !descriptor.descriptor_valid || descriptor.active ||
       descriptor.guard_verified ||
       !tgd783_verify(tis782_gdt,sizeof tis782_gdt,tis782_tss,
                     TIS782_TSS_SIZE,&descriptor) ||
       descriptor.tss_selector!=(uint16_t)(gdtr.limit+1u))
        paging_fail("783 TSS GDT descriptor audit");
    uint16_t tr_check=0;
    __asm__ volatile("str %0" : "=r"(tr_check));
    if(tr_check!=tr783 || !p->old_root)
        paging_fail("783 TR changed");
    arch_puts("STYLEOS TGD 783 DESCRIPTOR");
    put_field(" cr3=",p->old_root);
    put_field(" gdt-origin=",gdtr.base);
    put_field(" old-limit=",gdtr.limit);
    put_field(" gdt-target=",tclaims.gdt);
    put_field(" tss-base=",descriptor.tss_base);
    put_field(" cs=",cs783);
    put_field(" tss-selector=",descriptor.tss_selector);
    put_field(" new-limit=",descriptor.new_gdt_limit);
    put_field(" rsp0-top=",descriptor.rsp0_top);
    put_field(" ist1-top=",descriptor.ist1_top);
    arch_puts(" valid=1 tr=0 lgdt=0 ltr=0 guard=unproven active=0 no-cr3-switch=1\n");
#ifdef STYLEOS_TEST_TSS_LTR_784
    tlt784_terminal_trial(p,&descriptor,tis782_gdt,tis782_tss);
#endif
#ifdef STYLEOS_TEST_IST_HW_786
    s786_terminal_trial(p);
#endif
#ifdef STYLEOS_TEST_GUARD_PF_787
    s787_terminal_trial(p);
#endif
}
