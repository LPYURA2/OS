#include "styleos/disk_app_arch.h"
#include "styleos/arch.h"
#include "styleos/exception.h"

/* Only the fixed, admitted QEMU virt Image uses pointer values as physical
 * addresses. This is not a general pointer-to-PA interface. */
#define ARM_IMAGE_BASE UINT64_C(0x40080000)
#define ARM_UART UINT64_C(0x09000000)
#define ARM_ADDRESS_MASK UINT64_C(0x0000fffffffff000)
#define ARM_PXN (UINT64_C(1)<<53)
#define ARM_UXN (UINT64_C(1)<<54)
#define ARM_NORMAL (UINT64_C(3)|(UINT64_C(3)<<8)|(UINT64_C(1)<<10))
#define ARM_KERNEL (ARM_NORMAL|ARM_UXN)
#define ARM_USER_CODE (ARM_NORMAL|(UINT64_C(3)<<6)|ARM_PXN)
#define ARM_USER_STACK (ARM_NORMAL|(UINT64_C(1)<<6)|ARM_PXN|ARM_UXN)
#define ARM_DEVICE (UINT64_C(3)|(UINT64_C(1)<<2)|(UINT64_C(2)<<8)| \
                    (UINT64_C(1)<<10)|ARM_PXN|ARM_UXN)
#define ARM_SCTLR UINT64_C(0x30d00801)
#define ARM_MAGIC UINT64_C(0x6469736b41524d38)

extern const unsigned char image_start[],__image_end[],aarch64_vectors[];
extern enum da_arch_result da_arm_enter(struct da_context *,uint64_t,uint64_t);

struct arm_scratch {
    uint64_t dispatcher_sp; /* Assembly offset zero: saved trusted call frame. */
    struct da_context *context;
    struct da_event *event;
    uint64_t active;
    uint64_t roots[DA_ROOTS];
    uint64_t tcr,mair,sctlr,vectors,bits,magic;
    const struct boot_memory_evidence *evidence;
    struct da_info info;
};
_Static_assert(offsetof(struct arm_scratch,dispatcher_sp)==0,"dispatcher offset");
_Static_assert(sizeof(struct arm_scratch)<=DA_ARCH_CONTROL_BYTES,"arch scratch");
_Static_assert(DA_ARCH_CONTROL_OFFSET==12288,"assembly scratch offset");

struct arm_raw_frame {
    uint64_t gpr[31],sp,pc,status,esr,far,root,kernel_sp,vector,reserved;
};
_Static_assert(sizeof(struct arm_raw_frame)==320,"full ARM trap frame");
_Static_assert(offsetof(struct arm_raw_frame,sp)==248,"raw SP offset");
_Static_assert(offsetof(struct arm_raw_frame,root)==288,"raw root offset");

static struct arm_scratch *scratch(void)
{ return (struct arm_scratch *)(void *)(styleos_disk_app_control+DA_ARCH_CONTROL_OFFSET); }
static uint64_t pointer(const void *p) { return (uint64_t)(uintptr_t)p; }
static int inside(struct boot_range r,uint64_t start,uint64_t bytes)
{ return r.start<r.end && bytes && start>=r.start && start<r.end && bytes<=r.end-start; }
static int overlap(struct boot_range a,struct boot_range b)
{ return a.start<b.end && b.start<a.end; }
static int reserved(const struct boot_memory_evidence *e,uint64_t start,uint64_t bytes)
{
    if (!e || !e->map || !bytes || bytes>UINT64_MAX-start ||
        e->map->nr_reserved>BOOT_MEMORY_INPUT_MAX ||
        e->map->nr_candidates>BOOT_MEMORY_RANGE_MAX) return 0;
    struct boot_range r={start,start+bytes};
    for (size_t n=0;n<e->map->nr_candidates;++n)
        if (overlap(r,e->map->candidates[n])) return 0;
    uint64_t at=start;
    for (size_t n=0;n<e->map->nr_reserved && at<r.end;++n) {
        struct boot_range owner=e->map->reserved[n];
        if (owner.start>at) return 0;
        if (owner.end>at) at=owner.end;
    }
    return at>=r.end;
}
static uint64_t read_root(void)
{ uint64_t v;__asm__ volatile("mrs %0,ttbr0_el1":"=r"(v));return v; }
static void switch_root(uint64_t root)
{
    __asm__ volatile("dsb sy\nmsr ttbr0_el1,%0\nisb\ntlbi vmalle1\ndsb sy\nisb"
                     ::"r"(root):"memory");
}

enum da_arch_result da_arch_observe(struct da_machine *out)
{
    if (!out) return DA_ARCH_ARGUMENT;
    uint64_t sctlr,tcr,mair,el,daif,vbar,upper;
    __asm__ volatile("mrs %0,sctlr_el1":"=r"(sctlr));
    __asm__ volatile("mrs %0,tcr_el1":"=r"(tcr));
    __asm__ volatile("mrs %0,mair_el1":"=r"(mair));
    __asm__ volatile("mrs %0,CurrentEL":"=r"(el));
    __asm__ volatile("mrs %0,DAIF":"=r"(daif));
    __asm__ volatile("mrs %0,vbar_el1":"=r"(vbar));
    __asm__ volatile("mrs %0,ttbr1_el1":"=r"(upper));
    struct arm_scratch *s=scratch();
    *out=(struct da_machine){.root=read_root(),.mode=sctlr,.control0=tcr,
        .control1=mair,.control2=el,.control3=daif,.vectors=vbar,.upper_root=upper,
        .active=s->active,.dispatcher_sp=s->dispatcher_sp,
        .context_live=!!s->context || !!s->event};
    return DA_ARCH_OK;
}
static enum da_arch_result map_page(unsigned root,unsigned *used,uint64_t va,
                                     uint64_t pa,uint64_t attributes)
{
    if (root>=DA_ROOTS || !used || !*used || *used>DA_TABLE_PAGES ||
        ((va|pa)&(DA_PAGE-1u)) || va>=(UINT64_C(1)<<48) ||
        pa>=(UINT64_C(1)<<scratch()->bits)) return DA_ARCH_MAPPING;
    unsigned table=0;uint64_t base=pointer(styleos_disk_app_tables[root]);
    for (unsigned level=0;level<3;++level) {
        unsigned index=(unsigned)((va>>(39u-9u*level))&511u);
        uint64_t entry=styleos_disk_app_tables[root][table][index];
        if (!entry) {
            if (*used==DA_TABLE_PAGES) return DA_ARCH_FULL;
            unsigned child=(*used)++;
            styleos_disk_app_tables[root][table][index]=(base+child*DA_PAGE)|3u;
            table=child;
        } else {
            uint64_t child=entry&ARM_ADDRESS_MASK;
            if ((entry&~ARM_ADDRESS_MASK)!=3u || child<base ||
                child-base>=*used*(uint64_t)DA_PAGE) return DA_ARCH_MAPPING;
            table=(unsigned)((child-base)/DA_PAGE);
        }
    }
    unsigned index=(unsigned)((va>>12)&511u);
    if (styleos_disk_app_tables[root][table][index]) return DA_ARCH_MAPPING;
    styleos_disk_app_tables[root][table][index]=pa|attributes;
    return DA_ARCH_OK;
}
static uint64_t expected_leaf(const struct boot_memory_evidence *e,unsigned root,uint64_t va)
{
    uint64_t end=(e->live.image.end+DA_PAGE-1u)&~UINT64_C(4095);
    if (va>=e->live.image.start && va<end) return va|ARM_KERNEL;
    if (va==ARM_UART) return ARM_UART|ARM_DEVICE;
    if (root==DA_ROOT_SAFE) return 0;
    if (va==DA_CODE_VA) return pointer(styleos_disk_app_code)|ARM_USER_CODE;
    if (va==DA_STACK_VA) return pointer(styleos_disk_app_user_stack)|ARM_USER_STACK;
    return 0;
}
/* Every nonzero descriptor in every reachable table is inspected. Reject
 * loops, table aliases, blocks, extra leaves, foreign aliases and loose AP/XN.
 * Unused reserve pages must remain entirely zero. */
static enum da_arch_result verify_table(const struct boot_memory_evidence *e,
    unsigned root,unsigned used,unsigned table,unsigned level,uint64_t prefix,
    unsigned char seen[DA_TABLE_PAGES],size_t *leaves)
{
    if (table>=used || level>3 || seen[table]) return DA_ARCH_MAPPING;
    seen[table]=1;uint64_t base=pointer(styleos_disk_app_tables[root]);
    for (unsigned index=0;index<512;++index) {
        uint64_t entry=((volatile uint64_t (*)[512])styleos_disk_app_tables[root])[table][index];
        if (!entry) continue;
        uint64_t va=prefix|((uint64_t)index<<(39u-9u*level));
        if (level<3) {
            uint64_t child=entry&ARM_ADDRESS_MASK;
            if ((entry&~ARM_ADDRESS_MASK)!=3u || child<base ||
                child-base>=used*(uint64_t)DA_PAGE) return DA_ARCH_MAPPING;
            enum da_arch_result error=verify_table(e,root,used,
                (unsigned)((child-base)/DA_PAGE),level+1u,va,seen,leaves);
            if (error!=DA_ARCH_OK) return error;
        } else {
            uint64_t expected=expected_leaf(e,root,va);
            if (!expected || entry!=expected) return DA_ARCH_MAPPING;
            ++*leaves;
        }
    }
    return DA_ARCH_OK;
}
static enum da_arch_result verify_root(const struct boot_memory_evidence *e,
                                       unsigned root,unsigned used)
{
    unsigned char seen[DA_TABLE_PAGES]={0};size_t leaves=0;
    enum da_arch_result error=verify_table(e,root,used,0,0,0,seen,&leaves);
    if (error!=DA_ARCH_OK) return error;
    for (unsigned n=0;n<DA_TABLE_PAGES;++n) {
        if (n<used && !seen[n]) return DA_ARCH_MAPPING;
        if (n>=used) for (unsigned j=0;j<512;++j)
            if (((volatile uint64_t (*)[512])styleos_disk_app_tables[root])[n][j])
                return DA_ARCH_MAPPING;
    }
    uint64_t end=(e->live.image.end+DA_PAGE-1u)&~UINT64_C(4095);
    size_t expected=(size_t)((end-e->live.image.start)/DA_PAGE)+1u+
                    (root==DA_ROOT_SAFE?0u:2u);
    return leaves==expected?DA_ARCH_OK:DA_ARCH_MAPPING;
}

enum da_arch_result da_arch_prepare(const struct boot_memory_evidence *e,
    const struct da_info *info,struct da_platform *p)
{
    if (!e || !e->map || !info || !p) return DA_ARCH_ARGUMENT;
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
    if (el!=4 || spsel!=1 || daif!=UINT64_C(0x3c0) || (sp&15u) ||
        !inside(e->live.stack,sp,16u) ||
        (sctlr&((UINT64_C(1)<<25)|(UINT64_C(1)<<24)|(UINT64_C(1)<<12)|5u)) ||
        ((mmfr0>>28)&15u)!=0 || ips>=sizeof widths/sizeof widths[0])
        return DA_ARCH_PLATFORM;
    if (e->live.image.start!=ARM_IMAGE_BASE ||
        e->live.image.start!=pointer(image_start) || e->live.image.end!=pointer(__image_end) ||
        e->live.image.end>=(UINT64_C(1)<<widths[ips]) ||
        e->live.image.end>UINT64_MAX-(DA_PAGE-1u) ||
        vbar!=pointer(aarch64_vectors) || !inside(e->live.image,vbar,2048u) ||
        !inside(e->live.image,pointer(styleos_disk_app_vectors),2048u) ||
        pointer(styleos_disk_app_vectors_end)-pointer(styleos_disk_app_vectors)!=2048u ||
        (pointer(styleos_disk_app_vectors)&2047u) ||
        !reserved(e,vbar,2048u) || !reserved(e,pointer(styleos_disk_app_vectors),2048u))
        return DA_ARCH_PLATFORM;
    const volatile uint32_t *h32=(const volatile uint32_t *)(uintptr_t)image_start;
    const volatile uint64_t *h64=(const volatile uint64_t *)(uintptr_t)image_start;
    if (h32[14]!=UINT32_C(0x644d5241) || h64[1]!=UINT64_C(0x80000) ||
        h64[2]!=e->live.image.end-e->live.image.start) return DA_ARCH_PLATFORM;
    struct boot_range objects[]={
        {pointer(styleos_disk_app_tables),pointer(styleos_disk_app_tables)+sizeof styleos_disk_app_tables},
        {pointer(styleos_disk_app_control),pointer(styleos_disk_app_control)+DA_CONTROL_BYTES},
        {pointer(styleos_disk_app_entry_stack),pointer(styleos_disk_app_entry_stack)+DA_ENTRY_STACK_BYTES},
        {pointer(styleos_disk_app_code),pointer(styleos_disk_app_code)+DA_PAGE},
        {pointer(styleos_disk_app_user_stack),pointer(styleos_disk_app_user_stack)+DA_PAGE}};
    for (size_t i=0;i<sizeof objects/sizeof objects[0];++i) {
        struct boot_range r=objects[i];
        if (r.start>=r.end || ((r.start|r.end)&4095u) ||
            !inside(e->live.image,r.start,r.end-r.start) || !reserved(e,r.start,r.end-r.start) ||
            overlap(r,e->live.stack) || overlap(r,e->live.handoff) ||
            overlap(r,e->live.source) || overlap(r,e->workspace)) return DA_ARCH_RANGE;
        for (size_t j=0;j<i;++j) if (overlap(r,objects[j])) return DA_ARCH_OVERLAP;
    }
    struct boot_range common={objects[1].start,objects[1].start+DA_ARCH_CONTROL_OFFSET};
    if (!inside(common,pointer(info),sizeof *info) || !inside(common,pointer(p),sizeof *p) ||
        !info->code_length || info->code_length>DA_PAGE || (info->code_length&3u) ||
        (info->entry_offset&3u) || info->entry_offset>=info->code_length ||
        (info->fault_offset&3u) || info->fault_offset>=info->code_length ||
        (info->exit_offset&3u) || info->exit_offset>=info->code_length)
        return DA_ARCH_RANGE;
    uint64_t image_end=(e->live.image.end+DA_PAGE-1u)&~UINT64_C(4095);
    if (!reserved(e,e->live.image.start,image_end-e->live.image.start)) return DA_ARCH_RANGE;

    /* Guest has already drained all synchronous VirtIO reads. With M/C/I off,
     * copying used physical Image storage. Complete writes, invalidate old
     * instruction state, and synchronize before mapping that page executable. */
    __asm__ volatile("dsb sy\nic iallu\ndsb sy\nisb":::"memory");
    struct arm_scratch *s=scratch();
    *s=(struct arm_scratch){.active=UINT64_MAX,
        .tcr=UINT64_C(16)|(UINT64_C(3)<<12)|(UINT64_C(16)<<16)|
             (UINT64_C(1)<<23)|(UINT64_C(2)<<30)|((uint64_t)ips<<32),
        .mair=UINT64_C(0x44),.sctlr=ARM_SCTLR,.vectors=pointer(styleos_disk_app_vectors),
        .bits=widths[ips],.magic=ARM_MAGIC,.evidence=e,.info=*info};
    for (unsigned root=0;root<DA_ROOTS;++root) {
        for (unsigned n=0;n<DA_TABLE_PAGES;++n)
            for (unsigned j=0;j<512;++j) styleos_disk_app_tables[root][n][j]=0;
        unsigned used=1;
        for (uint64_t at=e->live.image.start;at<image_end;at+=DA_PAGE) {
            enum da_arch_result error=map_page(root,&used,at,at,ARM_KERNEL);
            if (error!=DA_ARCH_OK) return error;
        }
        enum da_arch_result error=map_page(root,&used,ARM_UART,ARM_UART,ARM_DEVICE);
        if (error!=DA_ARCH_OK) return error;
        if (root==DA_ROOT_USER) {
            error=map_page(root,&used,DA_CODE_VA,pointer(styleos_disk_app_code),ARM_USER_CODE);
            if (error!=DA_ARCH_OK) return error;
            error=map_page(root,&used,DA_STACK_VA,pointer(styleos_disk_app_user_stack),ARM_USER_STACK);
            if (error!=DA_ARCH_OK) return error;
        }
        error=verify_root(e,root,used);if (error!=DA_ARCH_OK) return error;
        s->roots[root]=pointer(styleos_disk_app_tables[root]);
        p->roots[root]=s->roots[root];p->table_pages[root]=used;
    }
    uint64_t safe=s->roots[DA_ROOT_SAFE];
    __asm__ volatile("dsb sy\nmsr mair_el1,%0\nmsr tcr_el1,%1\n"
        "msr ttbr0_el1,%2\nmsr ttbr1_el1,xzr\nmsr vbar_el1,%3\nisb\n"
        "tlbi vmalle1\ndsb sy\nisb\nmsr sctlr_el1,%4\nisb"
        ::"r"(s->mair),"r"(s->tcr),"r"(safe),"r"(s->vectors),"r"(s->sctlr):"memory");
    if (da_arch_observe(&p->safe)!=DA_ARCH_OK || p->safe.root!=safe || p->safe.upper_root ||
        p->safe.mode!=s->sctlr || p->safe.control0!=s->tcr ||
        p->safe.control1!=s->mair || p->safe.control2!=4u ||
        p->safe.control3!=UINT64_C(0x3c0) || p->safe.vectors!=s->vectors ||
        p->safe.active!=UINT64_MAX || p->safe.context_live)
        da_fail("ARM safe activation");
    return DA_ARCH_OK;
}

static int safe_machine(const struct da_machine *m,const struct arm_scratch *s)
{
    return m->root==s->roots[DA_ROOT_SAFE] && !m->upper_root &&
        m->mode==s->sctlr && m->control0==s->tcr && m->control1==s->mair &&
        m->control2==4u && m->control3==UINT64_C(0x3c0) && m->vectors==s->vectors;
}
enum da_arch_result da_arch_run(struct da_context *context,struct da_event *event)
{
    struct arm_scratch *s=scratch();
    if (!context || !event || s->magic!=ARM_MAGIC || s->active!=UINT64_MAX ||
        s->context || s->event) return DA_ARCH_CONTEXT;
    struct da_machine machine;
    if (da_arch_observe(&machine)!=DA_ARCH_OK || !safe_machine(&machine,s))
        return DA_ARCH_PLATFORM;
    struct boot_range control={pointer(styleos_disk_app_control),
        pointer(styleos_disk_app_control)+DA_ARCH_CONTROL_OFFSET};
    if (!inside(control,pointer(context),sizeof *context) ||
        !inside(control,pointer(event),sizeof *event) ||
        context->pc!=DA_CODE_VA+s->info.entry_offset ||
        context->sp!=DA_STACK_VA+DA_PAGE || context->status!=UINT64_C(0x3c0) ||
        context->cs || context->ss || context->gpr[0]>1u)
        return DA_ARCH_CONTEXT;
    s->context=context;s->event=event;s->active=context->gpr[0];
    enum da_arch_result result=da_arm_enter(context,s->roots[DA_ROOT_USER],
        pointer(styleos_disk_app_entry_stack)+DA_ENTRY_STACK_BYTES);
    if (result!=DA_ARCH_OK || da_arch_observe(&machine)!=DA_ARCH_OK ||
        !safe_machine(&machine,s) || s->active!=UINT64_MAX || s->context || s->event)
        return DA_ARCH_CONTEXT;
    return DA_ARCH_OK;
}

void da_arm_capture(const struct arm_raw_frame *raw)
{
    struct arm_scratch *s=scratch();
    struct boot_range entry={pointer(styleos_disk_app_entry_stack),
        pointer(styleos_disk_app_entry_stack)+DA_ENTRY_STACK_BYTES};
    if (s->magic!=ARM_MAGIC || s->active>1u || !s->context || !s->event ||
        pointer(raw)!=entry.end-sizeof *raw || !inside(entry,pointer(raw),sizeof *raw) ||
        raw->kernel_sp!=pointer(raw) || raw->vector!=8u || raw->reserved ||
        (raw->status&~UINT64_C(0xf00003c0)) ||
        (raw->status&UINT64_C(0x3c0))!=UINT64_C(0x3c0) ||
        raw->sp!=DA_STACK_VA+DA_PAGE || raw->root!=s->roots[DA_ROOT_USER] ||
        read_root()!=raw->root)
        da_fail("ARM user exception origin");
    enum da_event_kind kind=DA_EVENT_NONE;
    if (s->active==0 && raw->esr==UINT64_C(0x92000006) &&
        raw->far==DA_FAULT_VA && raw->pc==DA_CODE_VA+s->info.fault_offset)
        kind=DA_EVENT_FAULT;
    else if (s->active==1 && raw->esr==UINT64_C(0x56000002) &&
        raw->pc==DA_CODE_VA+s->info.exit_offset+4u &&
        raw->gpr[0]==s->info.exit_code && raw->gpr[19]==s->info.marker)
        kind=DA_EVENT_EXIT;
    else da_fail("ARM unexpected user exception");
    for (unsigned n=0;n<31;++n) s->context->gpr[n]=raw->gpr[n];
    s->context->sp=raw->sp;s->context->pc=raw->pc;s->context->status=raw->status;
    s->context->cs=0;s->context->ss=0;
    *s->event=(struct da_event){.kind=kind,.vector=raw->vector,.syndrome=raw->esr,
        .fault_va=kind==DA_EVENT_FAULT?raw->far:0,
        .active_root=raw->root,.kernel_sp=raw->kernel_sp};
    /* No ERET follows this exception: both events terminate this invocation.
     * Keep bookkeeping live until assembly has restored dispatcher SP. */
    switch_root(s->roots[DA_ROOT_SAFE]);
    if (read_root()!=s->roots[DA_ROOT_SAFE]) da_fail("ARM safe return root");
}

void da_arm_finish(void)
{
    struct arm_scratch *s=scratch();uint64_t sp;
    __asm__ volatile("mov %0,sp":"=r"(sp));
    if (s->magic!=ARM_MAGIC || s->active>1u || !s->context || !s->event ||
        read_root()!=s->roots[DA_ROOT_SAFE] || !s->dispatcher_sp ||
        !inside(s->evidence->live.stack,sp,16u) || sp>s->dispatcher_sp ||
        !inside(s->evidence->live.stack,s->dispatcher_sp,96u))
        da_fail("ARM dispatcher return stack");
    s->active=UINT64_MAX;s->context=0;s->event=0;s->dispatcher_sp=0;
}

void da_arch_dump(const struct da_platform *p)
{
    struct arm_scratch *s=scratch();
    if (!p || s->magic!=ARM_MAGIC) da_fail("ARM table dump state");
    for (unsigned r=0;r<DA_ROOTS;++r) {
        if (p->roots[r]!=pointer(styleos_disk_app_tables[r]) ||
            !p->table_pages[r] || p->table_pages[r]>DA_TABLE_PAGES ||
            verify_root(s->evidence,r,p->table_pages[r])!=DA_ARCH_OK)
            da_fail("ARM live table closure");
        const char *name=r==DA_ROOT_SAFE?"SAFE":"APP";
        arch_puts("STYLEOS DISKAPP ROOT id=");arch_puts(name);
        da_field(" pa=",p->roots[r]);da_field(" pages=",p->table_pages[r]);arch_putc('\n');
        uint64_t entries=0;
        for (unsigned t=0;t<DA_TABLE_PAGES;++t) for (unsigned i=0;i<512;++i) {
            uint64_t entry=((volatile uint64_t (*)[512])styleos_disk_app_tables[r])[t][i];
            if (!entry) continue;
            if (t>=p->table_pages[r]) da_fail("ARM unused table not zero");
            ++entries;arch_puts("STYLEOS DISKAPP TABLE id=");arch_puts(name);
            da_field(" table=",p->roots[r]+(uint64_t)t*DA_PAGE);
            da_field(" index=",i);da_field(" entry=",entry);arch_putc('\n');
        }
        arch_puts("STYLEOS DISKAPP TABLES END id=");arch_puts(name);
        da_field(" entries=",entries);arch_putc('\n');
    }
}
const char *da_arch_error_name(enum da_arch_result error)
{
    static const char *const names[]={"ok","argument","platform","range",
        "overlap","full","context","mapping"};
    return (unsigned)error<sizeof names/sizeof names[0]?names[error]:"unknown";
}
