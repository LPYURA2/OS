#include "styleos/user_processes.h"
#include "styleos/exception.h"

/* T-P1.7 is a finite QEMU-virt experiment. These addresses are justified only
 * for the checked fixed Image with MMU off, not arbitrary C pointer-to-PA. */
#define ARM_IMAGE_BASE UINT64_C(0x40080000)
#define ARM_UART UINT64_C(0x09000000)
#define ARM_ADDRESS_MASK UINT64_C(0x0000fffffffff000)
#define ARM_PXN (UINT64_C(1) << 53)
#define ARM_UXN (UINT64_C(1) << 54)
#define ARM_NORMAL (UINT64_C(3) | (UINT64_C(3) << 8) | (UINT64_C(1) << 10))
#define ARM_KERNEL (ARM_NORMAL | ARM_UXN)
#define ARM_USER_CODE (ARM_NORMAL | (UINT64_C(3) << 6) | ARM_PXN)
#define ARM_USER_DATA (ARM_NORMAL | (UINT64_C(1) << 6) | ARM_PXN | ARM_UXN)
#define ARM_DEVICE (UINT64_C(3) | (UINT64_C(1) << 2) | (UINT64_C(2) << 8) | \
                    (UINT64_C(1) << 10) | ARM_PXN | ARM_UXN)
#define ARM_SCTLR UINT64_C(0x30d00801)
#define ARM_MAGIC UINT64_C(0x7573657241524d37)

extern const unsigned char image_start[], __image_end[], aarch64_vectors[];
extern const unsigned char styleos_user_vectors[], styleos_user_vectors_end[];
extern const unsigned char styleos_user_code_a_end[], styleos_user_code_b_end[];
extern const unsigned char styleos_user_a_yield[], styleos_user_a_yield_resume[];
extern const unsigned char styleos_user_a_fault[], styleos_user_b_yield[];
extern const unsigned char styleos_user_b_yield_resume[], styleos_user_b_exit[];
extern enum up_error up_arm_enter(struct up_context *, uint64_t, uint64_t);

struct arm_scratch {
    uint64_t dispatcher_sp; /* Assembly offset0; original kernel-stack frame. */
    struct up_context *context;
    struct up_event *event;
    uint64_t active;
    uint64_t roots[UP_ROOTS];
    uint64_t tcr, mair, sctlr, vectors, bits, magic;
    const struct boot_memory_evidence *evidence;
};
_Static_assert(offsetof(struct arm_scratch,dispatcher_sp)==0,"dispatcher offset");
_Static_assert(sizeof(struct arm_scratch)<=UP_ARCH_CONTROL_BYTES,"arch scratch");
_Static_assert(UP_ARCH_CONTROL_OFFSET==12288,"assembly scratch offset");

struct arm_raw_frame {
    uint64_t gpr[31], sp, pc, status, esr, far, root, kernel_sp, vector, reserved;
};
_Static_assert(sizeof(struct arm_raw_frame)==320,"full ARM trap frame");
_Static_assert(offsetof(struct arm_raw_frame,sp)==248,"raw SP offset");
_Static_assert(offsetof(struct arm_raw_frame,root)==288,"raw root offset");

static struct arm_scratch *scratch(void)
{
    return (struct arm_scratch *)(void *)(styleos_user_control.bytes + UP_ARCH_CONTROL_OFFSET);
}

static uint64_t pointer(const void *p) { return (uint64_t)(uintptr_t)p; }
static int inside(struct boot_range r,uint64_t start,uint64_t bytes)
{
    return r.start<r.end && bytes && start>=r.start && start<r.end && bytes<=r.end-start;
}
static int overlap(struct boot_range a,struct boot_range b)
{
    return a.start<b.end && b.start<a.end;
}

/* Every complete range is baseline RESERVED and outside CANDIDATE before
 * reading/writing known Image storage. No candidate RAM is dereferenced. */
static int reserved(const struct boot_memory_evidence *e,uint64_t start,uint64_t bytes)
{
    if (!e || !e->map || !bytes || bytes>UINT64_MAX-start) return 0;
    struct boot_range r={start,start+bytes};
    for (size_t n=0;n<e->map->nr_candidates;++n)
        if (overlap(r,e->map->candidates[n])) return 0;
    uint64_t at=start;
    for (size_t n=0;n<e->map->nr_reserved && at<r.end;++n) {
        const struct boot_range owner=e->map->reserved[n];
        if (owner.start>at) return 0;
        if (owner.end>at) at=owner.end;
    }
    return at>=r.end;
}

static uint64_t read_root(void)
{
    uint64_t v;__asm__ volatile("mrs %0,ttbr0_el1":"=r"(v));return v;
}
static void switch_root(uint64_t root)
{
    __asm__ volatile("dsb sy\nmsr ttbr0_el1,%0\nisb\ntlbi vmalle1\ndsb sy\nisb"
                     ::"r"(root):"memory");
}

enum up_error up_arch_observe(struct up_machine *out)
{
    if (!out) return UP_ARGUMENT;
    uint64_t sctlr,tcr,mair,el,daif,vbar;
    __asm__ volatile("mrs %0,sctlr_el1":"=r"(sctlr));
    __asm__ volatile("mrs %0,tcr_el1":"=r"(tcr));
    __asm__ volatile("mrs %0,mair_el1":"=r"(mair));
    __asm__ volatile("mrs %0,CurrentEL":"=r"(el));
    __asm__ volatile("mrs %0,DAIF":"=r"(daif));
    __asm__ volatile("mrs %0,vbar_el1":"=r"(vbar));
    *out=(struct up_machine){.root=read_root(),.mode=sctlr,.control0=tcr,
        .control1=mair,.control2=el,.control3=daif,.vectors=vbar};
    return UP_OK;
}

static enum up_error map_page(unsigned root,unsigned *used,uint64_t va,
                               uint64_t pa,uint64_t attributes)
{
    if (root>=UP_ROOTS || !used || !*used || *used>UP_TABLE_PAGES ||
        (va|pa)&(UP_PAGE-1u) || va>=(UINT64_C(1)<<48) ||
        pa>=(UINT64_C(1)<<scratch()->bits)) return UP_MAPPING;
    unsigned table=0;
    const uint64_t base=pointer(styleos_user_tables[root]);
    for (unsigned level=0;level<3;++level) {
        unsigned index=(unsigned)((va>>(39u-9u*level))&511u);
        uint64_t entry=styleos_user_tables[root][table][index];
        if (!entry) {
            if (*used==UP_TABLE_PAGES) return UP_FULL;
            unsigned child=(*used)++;
            styleos_user_tables[root][table][index]=(base+child*UP_PAGE)|3u;
            table=child;
        } else {
            uint64_t child=entry&ARM_ADDRESS_MASK;
            if ((entry&~ARM_ADDRESS_MASK)!=3u || child<base ||
                child-base>=*used*(uint64_t)UP_PAGE) return UP_MAPPING;
            table=(unsigned)((child-base)/UP_PAGE);
        }
    }
    unsigned index=(unsigned)((va>>12)&511u);
    if (styleos_user_tables[root][table][index]) return UP_MAPPING;
    styleos_user_tables[root][table][index]=pa|attributes;
    return UP_OK;
}

static uint64_t expected_leaf(const struct boot_memory_evidence *e,
                               unsigned root,uint64_t va)
{
    uint64_t image_end=(e->live.image.end+UP_PAGE-1u)&~UINT64_C(4095);
    if (va>=e->live.image.start && va<image_end) return va|ARM_KERNEL;
    if (va==ARM_UART) return ARM_UART|ARM_DEVICE;
    if (root==UP_ROOT_SAFE) return 0;
    unsigned id=root-1u;
    if (va==UP_CODE_VA)
        return pointer(id?styleos_user_code_b:styleos_user_code_a)|ARM_USER_CODE;
    if (va==(id?UP_STACK_B_VA:UP_STACK_A_VA))
        return pointer(id?styleos_user_stack_b:styleos_user_stack_a)|ARM_USER_DATA;
    if (va==(id?UP_DATA_B_VA:UP_DATA_A_VA))
        return pointer(id?styleos_user_data_b:styleos_user_data_a)|ARM_USER_DATA;
    return 0;
}

/* Traverse every reachable descriptor, reject loops/aliases/huge entries and
 * compare the COMPLETE leaf set, including privilege and execute permissions.
 * An EL0 alias to any foreign or kernel physical page cannot pass this check. */
static enum up_error verify_table(const struct boot_memory_evidence *e,
    unsigned root,unsigned used,unsigned table,unsigned level,uint64_t prefix,
    unsigned char seen[UP_TABLE_PAGES],size_t *leaves)
{
    if (table>=used || level>3 || seen[table]) return UP_MAPPING;
    seen[table]=1;
    uint64_t base=pointer(styleos_user_tables[root]);
    for (unsigned index=0;index<512;++index) {
        uint64_t entry=styleos_user_tables[root][table][index];
        if (!entry) continue;
        uint64_t va=prefix|((uint64_t)index<<(39u-9u*level));
        if (level<3) {
            uint64_t child=entry&ARM_ADDRESS_MASK;
            if ((entry&~ARM_ADDRESS_MASK)!=3u || child<base ||
                child-base>=used*(uint64_t)UP_PAGE) return UP_MAPPING;
            enum up_error error=verify_table(e,root,used,(unsigned)((child-base)/UP_PAGE),
                                             level+1u,va,seen,leaves);
            if (error!=UP_OK) return error;
        } else {
            uint64_t expected=expected_leaf(e,root,va);
            if (!expected || entry!=expected) return UP_MAPPING;
            ++*leaves;
        }
    }
    return UP_OK;
}

static enum up_error verify_root(const struct boot_memory_evidence *e,
                                 unsigned root,unsigned used)
{
    unsigned char seen[UP_TABLE_PAGES]={0};size_t leaves=0;
    enum up_error error=verify_table(e,root,used,0,0,0,seen,&leaves);
    if (error!=UP_OK) return error;
    for (unsigned n=0;n<UP_TABLE_PAGES;++n) {
        if (n<used && !seen[n]) return UP_MAPPING;
        if (n>=used)
            for (unsigned j=0;j<512;++j)
                if (styleos_user_tables[root][n][j]) return UP_MAPPING;
    }
    uint64_t image_end=(e->live.image.end+UP_PAGE-1u)&~UINT64_C(4095);
    size_t expected=(size_t)((image_end-e->live.image.start)/UP_PAGE)+1u+
                    (root==UP_ROOT_SAFE?0u:3u);
    return leaves==expected?UP_OK:UP_MAPPING;
}

enum up_error up_arch_prepare(const struct boot_memory_evidence *e,struct up_platform *p)
{
    if (!e || !e->map || !p) return UP_ARGUMENT;
    uint64_t el,spsel,sp,mmfr0,daif,sctlr,vbar;
    __asm__ volatile("mrs %0,CurrentEL":"=r"(el));
    __asm__ volatile("mrs %0,SPSel":"=r"(spsel));
    __asm__ volatile("mov %0,sp":"=r"(sp));
    __asm__ volatile("mrs %0,id_aa64mmfr0_el1":"=r"(mmfr0));
    __asm__ volatile("mrs %0,DAIF":"=r"(daif));
    __asm__ volatile("mrs %0,sctlr_el1":"=r"(sctlr));
    __asm__ volatile("mrs %0,vbar_el1":"=r"(vbar));
    static const unsigned widths[]={32,36,40,42,44,48};
    unsigned ips=(unsigned)(mmfr0&15u);
    if (el!=4 || spsel!=1 || daif!=UINT64_C(0x3c0) ||
        (sp&15u) || !inside(e->live.stack,sp,16u) ||
        (sctlr&((UINT64_C(1)<<25)|(UINT64_C(1)<<24)|(UINT64_C(1)<<12)|5u)) ||
        ((mmfr0>>28)&15u)!=0 || ips>=sizeof widths/sizeof widths[0]) return UP_PLATFORM;
    if (e->live.image.start!=ARM_IMAGE_BASE ||
        e->live.image.start!=pointer(image_start) || e->live.image.end!=pointer(__image_end) ||
        e->live.image.end>=(UINT64_C(1)<<widths[ips]) ||
        vbar!=pointer(aarch64_vectors) || !inside(e->live.image,vbar,2048u) ||
        !inside(e->live.image,pointer(styleos_user_vectors),2048u) ||
        pointer(styleos_user_vectors_end)-pointer(styleos_user_vectors)!=2048u ||
        (pointer(styleos_user_vectors)&2047u) ||
        !reserved(e,vbar,2048u) || !reserved(e,pointer(styleos_user_vectors),2048u))
        return UP_PLATFORM;
    const volatile uint32_t *h32=(const volatile uint32_t *)(uintptr_t)image_start;
    const volatile uint64_t *h64=(const volatile uint64_t *)(uintptr_t)image_start;
    if (h32[14]!=UINT32_C(0x644d5241) || h64[1]!=UINT64_C(0x80000) ||
        h64[2]!=e->live.image.end-e->live.image.start) return UP_PLATFORM;
    struct boot_range objects[]={
        {pointer(styleos_user_tables),pointer(styleos_user_tables)+sizeof styleos_user_tables},
        {pointer(&styleos_user_control),pointer(&styleos_user_control)+UP_CONTROL_BYTES},
        {pointer(styleos_user_entry_stack),pointer(styleos_user_entry_stack)+UP_ENTRY_STACK_BYTES},
        {pointer(styleos_user_stack_a),pointer(styleos_user_stack_a)+UP_PAGE},
        {pointer(styleos_user_stack_b),pointer(styleos_user_stack_b)+UP_PAGE},
        {pointer(styleos_user_data_a),pointer(styleos_user_data_a)+UP_PAGE},
        {pointer(styleos_user_data_b),pointer(styleos_user_data_b)+UP_PAGE},
        {pointer(styleos_user_code_a),pointer(styleos_user_code_a_end)},
        {pointer(styleos_user_code_b),pointer(styleos_user_code_b_end)}};
    for (size_t i=0;i<sizeof objects/sizeof objects[0];++i) {
        struct boot_range r=objects[i];
        if (r.start>=r.end || ((r.start|r.end)&4095u) ||
            !inside(e->live.image,r.start,r.end-r.start) || !reserved(e,r.start,r.end-r.start) ||
            overlap(r,e->live.stack) || overlap(r,e->live.handoff) ||
            overlap(r,e->live.source) || overlap(r,e->workspace)) return UP_RANGE;
        for (size_t j=0;j<i;++j) if (overlap(r,objects[j])) return UP_OVERLAP;
    }
    if (objects[7].end-objects[7].start!=UP_PAGE ||
        objects[8].end-objects[8].start!=UP_PAGE ||
        e->live.image.end>UINT64_MAX-(UP_PAGE-1u)) return UP_RANGE;
    uint64_t image_end=(e->live.image.end+UP_PAGE-1u)&~UINT64_C(4095);
    if (!reserved(e,e->live.image.start,image_end-e->live.image.start)) return UP_RANGE;

    /* All new mutable storage and vector/code spans have now been admitted. */
    struct arm_scratch *s=scratch();
    *s=(struct arm_scratch){.active=UINT64_MAX,.tcr=UINT64_C(16)|(UINT64_C(3)<<12)|
        (UINT64_C(16)<<16)|(UINT64_C(1)<<23)|(UINT64_C(2)<<30)|((uint64_t)ips<<32),
        .mair=UINT64_C(0x44),.sctlr=ARM_SCTLR,.vectors=pointer(styleos_user_vectors),
        .bits=widths[ips],.magic=ARM_MAGIC,.evidence=e};
    for (unsigned root=0;root<UP_ROOTS;++root) {
        for (unsigned n=0;n<UP_TABLE_PAGES;++n)
            for (unsigned j=0;j<512;++j) styleos_user_tables[root][n][j]=0;
        unsigned used=1;
        for (uint64_t at=e->live.image.start;at<image_end;at+=UP_PAGE) {
            enum up_error error=map_page(root,&used,at,at,ARM_KERNEL);
            if (error!=UP_OK) return error;
        }
        enum up_error error=map_page(root,&used,ARM_UART,ARM_UART,ARM_DEVICE);
        if (error!=UP_OK) return error;
        if (root!=UP_ROOT_SAFE) {
            unsigned id=root-1u;
            error=map_page(root,&used,UP_CODE_VA,
                pointer(id?styleos_user_code_b:styleos_user_code_a),ARM_USER_CODE);
            if (error!=UP_OK) return error;
            error=map_page(root,&used,id?UP_STACK_B_VA:UP_STACK_A_VA,
                pointer(id?styleos_user_stack_b:styleos_user_stack_a),ARM_USER_DATA);
            if (error!=UP_OK) return error;
            error=map_page(root,&used,id?UP_DATA_B_VA:UP_DATA_A_VA,
                pointer(id?styleos_user_data_b:styleos_user_data_a),ARM_USER_DATA);
            if (error!=UP_OK) return error;
        }
        error=verify_root(e,root,used);if (error!=UP_OK) return error;
        s->roots[root]=pointer(styleos_user_tables[root]);
        p->roots[root]=s->roots[root];p->table_pages[root]=used;
    }
    p->entry[0]=p->entry[1]=UP_CODE_VA;
    p->yield_pc[0]=UP_CODE_VA+pointer(styleos_user_a_yield)-pointer(styleos_user_code_a);
    p->yield_pc[1]=UP_CODE_VA+pointer(styleos_user_b_yield)-pointer(styleos_user_code_b);
    p->resume_pc[0]=UP_CODE_VA+pointer(styleos_user_a_yield_resume)-pointer(styleos_user_code_a);
    p->resume_pc[1]=UP_CODE_VA+pointer(styleos_user_b_yield_resume)-pointer(styleos_user_code_b);
    p->fault_pc[0]=UP_CODE_VA+pointer(styleos_user_a_fault)-pointer(styleos_user_code_a);
    p->fault_pc[1]=0;p->exit_pc[0]=0;
    p->exit_pc[1]=UP_CODE_VA+pointer(styleos_user_b_exit)-pointer(styleos_user_code_b);
    up_range_line("OBJECT name=vectors",(struct boot_range){vbar,vbar+2048u});
    up_range_line("OBJECT name=user_vectors",(struct boot_range){s->vectors,s->vectors+2048u});
    uint64_t safe=s->roots[UP_ROOT_SAFE];
    __asm__ volatile("dsb sy\nmsr mair_el1,%0\nmsr tcr_el1,%1\n"
        "msr ttbr0_el1,%2\nmsr ttbr1_el1,xzr\nmsr vbar_el1,%3\nisb\n"
        "tlbi vmalle1\ndsb sy\nisb\nmsr sctlr_el1,%4\nisb"
        ::"r"(s->mair),"r"(s->tcr),"r"(safe),"r"(s->vectors),"r"(s->sctlr):"memory");
    uint64_t ttbr1;__asm__ volatile("mrs %0,ttbr1_el1":"=r"(ttbr1));
    if (up_arch_observe(&p->safe)!=UP_OK || p->safe.root!=safe || ttbr1 ||
        p->safe.mode!=s->sctlr || p->safe.control0!=s->tcr ||
        p->safe.control1!=s->mair || p->safe.control2!=4u ||
        p->safe.control3!=UINT64_C(0x3c0) || p->safe.vectors!=s->vectors)
        up_fail("ARM safe activation");
    return UP_OK;
}

enum up_error up_arch_run(unsigned id,struct up_context *context,struct up_event *event)
{
    struct arm_scratch *s=scratch();
    if (id>=UP_SLOTS || !context || !event || s->magic!=ARM_MAGIC ||
        s->active!=UINT64_MAX || read_root()!=s->roots[UP_ROOT_SAFE]) return UP_CONTEXT;
    struct up_machine machine;
    if (up_arch_observe(&machine)!=UP_OK || machine.mode!=s->sctlr ||
        machine.control0!=s->tcr || machine.control1!=s->mair ||
        machine.control2!=4u || machine.control3!=UINT64_C(0x3c0) ||
        machine.vectors!=s->vectors) return UP_PLATFORM;
    struct boot_range control={pointer(&styleos_user_control),
                              pointer(&styleos_user_control)+UP_ARCH_CONTROL_OFFSET};
    uint64_t stack=id?UP_STACK_B_VA:UP_STACK_A_VA;
    if (!inside(control,pointer(context),sizeof *context) ||
        !inside(s->evidence->live.stack,pointer(event),sizeof *event) ||
        context->pc<UP_CODE_VA || context->pc>=UP_CODE_VA+UP_PAGE || (context->pc&3u) ||
        context->sp<=stack || context->sp>stack+UP_PAGE || (context->sp&15u) ||
        (context->status&~UINT64_C(0xf00003c0)) ||
        (context->status&UINT64_C(0x3c0))!=UINT64_C(0x3c0) || context->cs || context->ss)
        return UP_CONTEXT;
    s->context=context;s->event=event;s->active=id;
    enum up_error result=up_arm_enter(context,s->roots[id+1u],
        pointer(styleos_user_entry_stack)+UP_ENTRY_STACK_BYTES);
    if (result!=UP_OK || s->active!=UINT64_MAX ||
        read_root()!=s->roots[UP_ROOT_SAFE]) return UP_CONTEXT;
    return UP_OK;
}

void up_arm_capture(const struct arm_raw_frame *raw)
{
    struct arm_scratch *s=scratch();
    struct boot_range entry={pointer(styleos_user_entry_stack),
        pointer(styleos_user_entry_stack)+UP_ENTRY_STACK_BYTES};
    if (s->magic!=ARM_MAGIC || s->active>=UP_SLOTS || !s->context || !s->event ||
        pointer(raw)!=entry.end-sizeof *raw || !inside(entry,pointer(raw),sizeof *raw) ||
        raw->kernel_sp!=pointer(raw) || raw->vector!=8u || raw->reserved ||
        (raw->status&~UINT64_C(0xf00003c0)) ||
        (raw->status&UINT64_C(0x3c0))!=UINT64_C(0x3c0) ||
        raw->root!=s->roots[s->active+1u] || read_root()!=raw->root)
        up_fail("ARM user exception origin");
    unsigned ec=(unsigned)((raw->esr>>26)&63u);
    enum up_kind kind=UP_EVENT_NONE;
    if (ec==0x15u && (raw->esr&UINT64_C(0x1ffffff))==(raw->gpr[0]&0xffffu) &&
        (raw->gpr[0]==1u || raw->gpr[0]==2u))
        kind=raw->gpr[0]==1u?UP_YIELD:UP_EXIT;
    else if (ec==0x24u && raw->esr==UINT64_C(0x92000007)) kind=UP_FAULT;
    else up_fail("ARM unexpected user exception");
    for (unsigned n=0;n<31;++n) s->context->gpr[n]=raw->gpr[n];
    s->context->sp=raw->sp;s->context->pc=raw->pc;s->context->status=raw->status;
    s->context->cs=0;s->context->ss=0;
    *s->event=(struct up_event){.kind=kind,.vector=raw->vector,.syndrome=raw->esr,
        .fault_va=kind==UP_FAULT?raw->far:0,.active_root=raw->root,.kernel_sp=raw->kernel_sp};
    /* Switch to a root with NO EL0 leaves before returning to the trusted
     * dispatcher or releasing bookkeeping. Physical backing stays reserved. */
    switch_root(s->roots[UP_ROOT_SAFE]);
    if (read_root()!=s->roots[UP_ROOT_SAFE]) up_fail("ARM safe return root");
    s->active=UINT64_MAX;s->context=0;s->event=0;
}
