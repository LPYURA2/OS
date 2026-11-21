#include "styleos/user_processes.h"
#include "styleos/boot_pool.h"
#include "styleos/exception.h"
#include "styleos/arch.h"
#ifdef STYLEOS_TEST_NATIVE_TEA
#include "styleos/tea_768_demo.h"
#endif

/* Read-only admission for this inherited UEFI x64 profile.  The existing
 * Loaded Image extent is authoritative; each image VA is checked against the
 * active root before its known image-owned buffers are used as PA ranges.
 * Physical table reads retain ADR-0007/0008's narrow UEFI access assumption.
 * Every complete table page is RESERVED/non-CANDIDATE before its first read,
 * and a bounded identity closure checks that actual access afterwards.  This
 * does not enumerate all firmware dependencies or provide a general VA->PA
 * service.  No descriptor or translation register is written here. */
#define BIT(n) (UINT64_C(1) << (n))
#define PAGE UINT64_C(4096)
#define LOW_LIMIT BIT(47)
#define ENTRY_ADDRESS UINT64_C(0x000ffffffffff000)
#define TABLE_LIMIT 64u

struct descriptor_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct inherited_state {
    uint64_t mode,root,control0,control1,flags,cs;
    uint64_t idt_base,idt_bytes,gdt_base,gdt_bytes;
    uint64_t pat,pa_bits,gdt_pages,gdt_pa0,gdt_pa1;
};

struct observation {
    const struct boot_memory_evidence *evidence;
    struct inherited_state state;
    struct descriptor_pointer idtr, gdtr;
    uint64_t pa_mask, visited[TABLE_LIMIT];
    size_t nr_visited;
    int one_gib;
};

struct mapping { uint64_t pa; unsigned type; int writable, executable; };

static int inside(struct boot_range range, uint64_t start, uint64_t bytes)
{
    return range.start < range.end && bytes && start >= range.start &&
           start < range.end && bytes <= range.end - start;
}

static int overlaps(struct boot_range range, uint64_t page)
{
    return page < range.end && range.start < page + PAGE;
}

static uint64_t read_cr0(void)
{ uint64_t v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static uint64_t read_cr3(void)
{ uint64_t v; __asm__ volatile("mov %%cr3, %0" : "=r"(v)); return v; }
static uint64_t read_cr4(void)
{ uint64_t v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v; }
static uint64_t read_flags(void)
{ uint64_t v; __asm__ volatile("pushfq; popq %0" : "=r"(v)); return v; }
static uint64_t read_msr(uint32_t number)
{
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(number));
    return (uint64_t)low | (uint64_t)high << 32;
}
static void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b,
                  uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                     : "a"(leaf), "c"(0));
}

static int read_state(struct observation *o)
{
    uint32_t a, b, c, d;
    cpuid(0x80000000u, &a, &b, &c, &d);
    if (a < 0x80000008u) return 0;
    cpuid(0x80000001u, &a, &b, &c, &d);
    if (!(d & (1u << 20))) return 0;
    o->one_gib = !!(d & (1u << 26));
    cpuid(0x80000008u, &a, &b, &c, &d);
    struct inherited_state *s = &o->state;
    s->pa_bits = a & 255u;
    s->mode = read_cr0(); s->root = read_cr3();
    s->control0 = read_cr4(); s->control1 = read_msr(0xc0000080u);
    s->pat = read_msr(0x277u); s->flags = read_flags();
    __asm__ volatile("sgdt %0" : "=m"(o->gdtr));
    __asm__ volatile("sidt %0" : "=m"(o->idtr));
    uint16_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    s->cs = cs; s->gdt_base = o->gdtr.base;
    s->gdt_bytes = (uint64_t)o->gdtr.limit + 1u;
    s->idt_base = o->idtr.base; s->idt_bytes = (uint64_t)o->idtr.limit + 1u;
    const uint64_t unsupported = BIT(12) | BIT(17) | BIT(21) |
                                 BIT(22) | BIT(23) | BIT(24);
    if (s->pa_bits < 32 || s->pa_bits > 52 ||
        (s->mode & (BIT(0) | BIT(31))) != (BIT(0) | BIT(31)) ||
        (s->mode & (BIT(29) | BIT(30))) || !(s->control0 & BIT(5)) ||
        (s->control0 & unsupported) || (s->flags & (BIT(9) | BIT(10))) ||
        (s->control1 & (BIT(8) | BIT(10) | BIT(11))) !=
            (BIT(8) | BIT(10) | BIT(11)) || (s->pat & 255u) != 6u)
        return 0;
    o->pa_mask = (BIT(s->pa_bits) - 1u) & ENTRY_ADDRESS;
    return s->root && !(s->root & ~o->pa_mask) && s->root < LOW_LIMIT;
}

static int reserved_page(const struct observation *o, uint64_t page)
{
    const struct boot_memory_map *map = o->evidence->map;
    if (!map || (page & (PAGE - 1u)) || page > UINT64_MAX - PAGE ||
        map->nr_reserved > BOOT_MEMORY_INPUT_MAX ||
        map->nr_candidates > BOOT_MEMORY_RANGE_MAX) return 0;
    int reserved = 0;
    for (size_t i = 0; i < map->nr_reserved; ++i) {
        struct boot_range r = map->reserved[i];
        if (r.start >= r.end) return 0;
        if (r.start <= page && page + PAGE <= r.end) reserved = 1;
    }
    if (!reserved) return 0;
    for (size_t i = 0; i < map->nr_candidates; ++i) {
        struct boot_range r = map->candidates[i];
        if (r.start >= r.end || overlaps(r, page)) return 0;
    }
    return 1;
}

static int remember_table(struct observation *o, uint64_t page)
{
    if (!page || page >= LOW_LIMIT || (page & ~o->pa_mask) ||
        !reserved_page(o, page) || overlaps(o->evidence->live.image, page))
        return 0;
    for (size_t i = 0; i < o->nr_visited; ++i)
        if (o->visited[i] == page) return 1;
    if (o->nr_visited == TABLE_LIMIT) return 0;
    o->visited[o->nr_visited++] = page;
    return 1;
}

static int walk(struct observation *o, uint64_t va, struct mapping *mapping)
{
    if (va >= LOW_LIMIT) return 0;
    uint64_t page = o->state.root;
    int writable = 1, executable = 1;
    for (unsigned level = 0; level < 4; ++level) {
        /* No candidate or arbitrary physical read: only the active root and
         * actual child descriptors, admitted before dereferencing. */
        if (!remember_table(o, page)) return 0;
        const volatile uint64_t *table = (const volatile uint64_t *)(uintptr_t)page;
        const unsigned shift = 39u - level * 9u;
        const uint64_t entry = table[(va >> shift) & 511u];
        if (!(entry & 1u) || (entry & ENTRY_ADDRESS & ~o->pa_mask) ||
            (entry & UINT64_C(0x7800000000000000))) return 0;
        writable = writable && !!(entry & BIT(1));
        executable = executable && !(entry & BIT(63));
        const int large = level < 3 && !!(entry & BIT(7));
        if (large && (level == 0 || (level == 1 && !o->one_gib))) return 0;
        if (level == 3 || large) {
            const unsigned leaf_shift = large ? shift : 12u;
            const uint64_t offset = BIT(leaf_shift) - 1u;
            if (large && (entry & ENTRY_ADDRESS & offset & ~BIT(12))) return 0;
            const unsigned pat_bit = (unsigned)((entry >> (large ? 12u : 7u)) & 1u);
            const unsigned index = (unsigned)((entry >> 3) & 3u) | pat_bit << 2;
            *mapping = (struct mapping){
                .pa = (entry & o->pa_mask & ~offset) | (va & offset),
                .type = (unsigned)((o->state.pat >> (index * 8u)) & 255u),
                .writable = writable, .executable = executable};
            return 1;
        }
        if (entry & UINT64_C(0x18)) return 0;
        page = entry & o->pa_mask;
    }
    return 0;
}

static int image_identity(struct observation *o)
{
    const struct boot_range image = o->evidence->live.image;
    if (image.start >= image.end || (image.start & (PAGE - 1u)) ||
        (image.end & (PAGE - 1u)) || image.end > LOW_LIMIT) return 0;
    for (uint64_t va = image.start; va < image.end; va += PAGE) {
        struct mapping m;
        if (!reserved_page(o, va) || !walk(o, va, &m) ||
            m.pa != va || m.type != 6u) return 0;
    }
    return 1;
}

static int writable_image(struct observation *o, uint64_t start, uint64_t bytes)
{
    if (!inside(o->evidence->live.image, start, bytes)) return 0;
    const uint64_t end = start + bytes;
    for (uint64_t va = start & ~(PAGE - 1u); va < end; va += PAGE) {
        struct mapping m;
        if (!walk(o, va, &m) || m.pa != va || m.type != 6u || !m.writable)
            return 0;
    }
    return 1;
}

static uint64_t load_le(const volatile unsigned char *p, unsigned bytes)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i) value |= (uint64_t)p[i] << (8u * i);
    return value;
}

static int pe_header(const struct observation *o)
{
    const struct boot_range image = o->evidence->live.image;
    if (!inside(image, image.start, 64u)) return 0;
    const volatile unsigned char *base =
        (const volatile unsigned char *)(uintptr_t)image.start;
    if (load_le(base, 2) != 0x5a4du) return 0;
    const uint64_t offset = load_le(base + 60, 4);
    if (offset > image.end - image.start ||
        !inside(image, image.start + offset, 24u + 112u)) return 0;
    const volatile unsigned char *pe = base + offset;
    const uint64_t sections = load_le(pe + 6, 2), optional = load_le(pe + 20, 2);
    if (load_le(pe, 4) != 0x4550u || load_le(pe + 4, 2) != 0x8664u ||
        !sections || sections > 96u || optional < 112u ||
        load_le(pe + 24, 2) != 0x20bu ||
        load_le(pe + 24 + 56, 4) != image.end - image.start ||
        !inside(image, image.start + offset + 24u, optional + sections * 40u))
        return 0;
    return 1;
}

/* Everything below belongs exclusively to --test-user-processes. The narrow
 * inherited-root proof above is read-only and completes before the first
 * write to any new object, including the opaque control scratch. */
struct up_idt_gate {
    uint16_t low,selector;
    uint8_t ist,attributes;
    uint16_t middle;
    uint32_t high,reserved;
} __attribute__((packed));
struct up_tss {
    uint32_t reserved0;
    uint64_t rsp[3],reserved1,ist[7],reserved2;
    uint16_t reserved3,iomap;
} __attribute__((packed));
_Static_assert(sizeof(struct up_tss)==104,"hardware64bitTSS extent");
_Static_assert(offsetof(struct up_tss,rsp)==4,"TSS.RSP0 offset");
_Static_assert(offsetof(struct up_tss,iomap)==102,"TSS IO-map offset");
_Static_assert(sizeof(struct up_idt_gate)==16,"hardware gate extent");
_Alignas(4096) uint64_t styleos_user_x86_gdt[512];
_Alignas(4096) unsigned char styleos_user_x86_tss[4096];
_Alignas(4096) struct up_idt_gate styleos_user_x86_idt[256];
struct up_x86_control {
    uint64_t dispatcher_sp;
    struct up_context *context;
    struct up_event *event;
    uint64_t safe_root,selected_root,running;
    uint64_t prepared,cr0,cr4,efer,pat;
    struct boot_range image;
};
_Static_assert(offsetof(struct up_x86_control,safe_root)==24,"ASM safe root");
_Static_assert(offsetof(struct up_x86_control,selected_root)==32,"ASM selected root");
_Static_assert(sizeof(struct up_x86_control)<=UP_ARCH_CONTROL_BYTES,"arch scratch extent");
#define XCONTROL ((struct up_x86_control *)(void *)\
    (styleos_user_control.bytes+UP_ARCH_CONTROL_OFFSET))
extern void (*const styleos_isr_table[256])(void);
extern void styleos_user_x86_pf(void),styleos_user_x86_call(void);
extern void up_x86_descriptors(const struct descriptor_pointer *,
                               const struct descriptor_pointer *);
extern void up_x86_enter(struct up_context *,struct up_event *,uint64_t root);
extern const unsigned char styleos_user_a_yield[],styleos_user_a_yield_resume[];
extern const unsigned char styleos_user_a_fault[];
extern const unsigned char styleos_user_b_yield[],styleos_user_b_yield_resume[];
extern const unsigned char styleos_user_b_exit[];

static struct boot_range object(const void *p,uint64_t bytes)
{
    const uint64_t start=(uint64_t)(uintptr_t)p;
    return (struct boot_range){start,start+bytes};
}

static int disjoint(struct boot_range a,struct boot_range b)
{ return a.start<a.end && b.start<b.end && (a.end<=b.start || b.end<=a.start); }

static void clear_bytes(void *p,size_t bytes)
{
    unsigned char *out=p;
    for(size_t i=0;i<bytes;++i)out[i]=0;
}

static int admit_new_objects(struct observation *o)
{
    struct boot_range items[]={
        object(styleos_user_tables,sizeof(styleos_user_tables)),
        object(&styleos_user_control,sizeof(styleos_user_control)),
        object(styleos_user_entry_stack,UP_ENTRY_STACK_BYTES),
        object(styleos_user_stack_a,UP_PAGE),object(styleos_user_stack_b,UP_PAGE),
        object(styleos_user_data_a,UP_PAGE),object(styleos_user_data_b,UP_PAGE),
        object(styleos_user_code_a,UP_PAGE),object(styleos_user_code_b,UP_PAGE),
        object(styleos_user_x86_gdt,UP_PAGE),object(styleos_user_x86_tss,UP_PAGE),
        object(styleos_user_x86_idt,UP_PAGE)
#ifdef STYLEOS_TEST_NATIVE_TEA
        ,object(styleos_tea_768_code_b,NTE_PAGE)
#endif
    };
    const struct boot_memory_evidence *e=o->evidence;
    struct boot_range excluded[]={e->live.stack,e->live.handoff,e->live.source,
        e->workspace,boot_pool_storage_range(),boot_pool_metadata_range(),
        {o->state.idt_base,o->state.idt_base+o->state.idt_bytes}};
    for(size_t i=0;i<sizeof(items)/sizeof(items[0]);++i){
        struct boot_range r=items[i];
        if(r.start>=r.end || ((r.start|r.end)&(PAGE-1u)) ||
           !inside(e->live.image,r.start,r.end-r.start))return 0;
        if(i!=7u && i!=8u && !writable_image(o,r.start,r.end-r.start))return 0;
        for(size_t j=0;j<i;++j)if(!disjoint(r,items[j]))return 0;
        for(size_t j=0;j<sizeof(excluded)/sizeof(excluded[0]);++j)
            if(!disjoint(r,excluded[j]))return 0;
    }
    /* A loaded image placed at one of the fixed user aliases is unsupported:
     * no supervisor mapping may silently collide with user leaf policy. */
    const uint64_t va[]={UP_CODE_VA,UP_STACK_A_VA,UP_STACK_B_VA,UP_DATA_A_VA,UP_DATA_B_VA};
    for(size_t i=0;i<sizeof(va)/sizeof(va[0]);++i)
        if(!disjoint(e->live.image,(struct boot_range){va[i],va[i]+UP_PAGE}))return 0;
    /* Close the actually visited inherited table set after all storage walks.
     * Each PA page was already admitted RESERVED/non-CANDIDATE before read. */
    for(size_t i=0;i<o->nr_visited;++i){
        struct mapping m;
        if(!walk(o,o->visited[i],&m) || m.pa!=o->visited[i] || m.type!=6u)return 0;
    }
    return 1;
}

/* Known image-owned table storage only: there is no generic PA dereference.
 * Every new parent is allocated from this root's finite32-page reserve. */
static int add_leaf(unsigned root,unsigned *used,uint64_t va,uint64_t pa,
                    int user,int writable,int executable)
{
    uint64_t (*arena)[512]=styleos_user_tables[root];
    uint64_t *table=arena[0];
    for(unsigned level=0;level<3;++level){
        const unsigned index=(unsigned)(va>>(39u-9u*level))&511u;
        uint64_t entry=table[index];
        if(!entry){
            if(*used>=UP_TABLE_PAGES)return 0;
            uint64_t *child=arena[(*used)++];
            entry=(uint64_t)(uintptr_t)child|3u;
        }
        if(user)entry|=4u;
        table[index]=entry;
        const uint64_t address=entry&ENTRY_ADDRESS;
        const uint64_t start=(uint64_t)(uintptr_t)arena;
        if(address<start || address>=start+(uint64_t)*used*UP_PAGE ||
            (entry&BIT(7)))return 0;
        table=arena[(address-start)/UP_PAGE];
    }
    unsigned index=(unsigned)(va>>12)&511u;
    if(table[index])return 0;
    table[index]=pa|1u|(writable?2u:0u)|(user?4u:0u)|(executable?0u:BIT(63));
    return 1;
}

struct own_mapping { uint64_t pa; unsigned user,writable,executable; };
static int own_walk(unsigned root,unsigned used,uint64_t va,struct own_mapping *out)
{
    uint64_t (*arena)[512]=styleos_user_tables[root];
    const uint64_t start=(uint64_t)(uintptr_t)arena;
    const uint64_t *table=arena[0];
    unsigned user=1,writable=1,executable=1;
    for(unsigned level=0;level<4;++level){
        uint64_t entry=table[(va>>(39u-level*9u))&511u];
        if(!(entry&1u))return 0;
        user&=!!(entry&4u);writable&=!!(entry&2u);executable&=!(entry&BIT(63));
        if(level==3){
            *out=(struct own_mapping){(entry&ENTRY_ADDRESS)|(va&4095u),user,writable,executable};
            return 1;
        }
        uint64_t address=entry&ENTRY_ADDRESS;
        if((entry&BIT(7)) || address<start || address>=start+(uint64_t)used*UP_PAGE)
            return 0;
        table=arena[(address-start)/UP_PAGE];
    }
    return 0;
}

static int map_matches(unsigned root,unsigned used,uint64_t va,uint64_t pa,
                       unsigned user,unsigned write,unsigned execute)
{
    struct own_mapping m;
    return own_walk(root,used,va,&m) && m.pa==pa && m.user==user &&
        m.writable==write && m.executable==execute;
}

static int build_roots(const struct boot_memory_evidence *e,struct up_platform *p)
{
    clear_bytes(styleos_user_tables,sizeof(styleos_user_tables));
    for(unsigned root=0;root<UP_ROOTS;++root){
        unsigned used=1;
        for(uint64_t va=e->live.image.start;va<e->live.image.end;va+=UP_PAGE)
            if(!add_leaf(root,&used,va,va,0,1,1))return 0;
        if(root){
            unsigned id=root-1u;
            const unsigned char *code=id?styleos_user_code_b:styleos_user_code_a;
#ifdef STYLEOS_TEST_NATIVE_TEA
            if(id==1u)code=styleos_tea_768_code_b;
#endif
            const unsigned char *stack=id?styleos_user_stack_b:styleos_user_stack_a;
            const uint64_t *data=id?styleos_user_data_b:styleos_user_data_a;
            uint64_t stack_va=id?UP_STACK_B_VA:UP_STACK_A_VA;
            uint64_t data_va=id?UP_DATA_B_VA:UP_DATA_A_VA;
            if(!add_leaf(root,&used,UP_CODE_VA,(uint64_t)(uintptr_t)code,1,0,1) ||
               !add_leaf(root,&used,stack_va,(uint64_t)(uintptr_t)stack,1,1,0) ||
               !add_leaf(root,&used,data_va,(uint64_t)(uintptr_t)data,1,1,0))return 0;
            if(!map_matches(root,used,UP_CODE_VA,(uint64_t)(uintptr_t)code,1,0,1) ||
               !map_matches(root,used,stack_va,(uint64_t)(uintptr_t)stack,1,1,0) ||
               !map_matches(root,used,data_va,(uint64_t)(uintptr_t)data,1,1,0))return 0;
            struct own_mapping absent;
            if(own_walk(root,used,id?UP_DATA_A_VA:UP_DATA_B_VA,&absent) ||
               own_walk(root,used,id?UP_STACK_A_VA:UP_STACK_B_VA,&absent))return 0;
        }else{
            struct own_mapping absent;
            if(own_walk(root,used,UP_CODE_VA,&absent) ||
               own_walk(root,used,UP_DATA_A_VA,&absent) ||
               own_walk(root,used,UP_DATA_B_VA,&absent))return 0;
        }
        for(uint64_t va=e->live.image.start;va<e->live.image.end;va+=UP_PAGE)
            if(!map_matches(root,used,va,va,0,1,1))return 0;
        p->roots[root]=(uint64_t)(uintptr_t)styleos_user_tables[root];
        p->table_pages[root]=used;
    }
    return 1;
}

static int inherited_idt_compatible(const struct observation *o)
{
    const uint64_t cs=o->state.cs;
    /* Reserve our first seven entries; retain a compatible kernel-code alias
     * at the original selector during LGDT->LIDT. Unsupported collisions are
     * refused under the still intact inherited descriptor context. */
    if(!cs || (cs&7u) || cs>4088u || (cs!=8u && cs<56u))return 0;
    const volatile unsigned char *gates=(const volatile unsigned char *)(uintptr_t)o->state.idt_base;
    for(unsigned i=0;i<256;++i){
        uint64_t low=load_le(gates+16u*i,8),high=load_le(gates+16u*i+8u,8);
        uint64_t target=(low&65535u)|((low>>48)<<16)|(high<<32);
        if(((low>>16)&65535u)!=cs || ((low>>32)&255u) ||
           ((low>>40)&255u)!=0x8eu || (high>>32) ||
           target!=(uint64_t)(uintptr_t)styleos_isr_table[i])return 0;
    }
    return 1;
}

static void build_descriptors(uint16_t retained_cs)
{
    clear_bytes(styleos_user_x86_gdt,sizeof(styleos_user_x86_gdt));
    clear_bytes(styleos_user_x86_tss,sizeof(styleos_user_x86_tss));
    clear_bytes(styleos_user_x86_idt,sizeof(styleos_user_x86_idt));
    /* Accessed is preset in every segment. Only LTR's busy bit is hardware
     * written, in our own writable image page. No firmware GDT store occurs. */
    styleos_user_x86_gdt[1]=UINT64_C(0x00af9b000000ffff);
    styleos_user_x86_gdt[2]=UINT64_C(0x00cf93000000ffff);
    styleos_user_x86_gdt[3]=UINT64_C(0x00cff3000000ffff);
    styleos_user_x86_gdt[4]=UINT64_C(0x00affb000000ffff);
    if(retained_cs!=8u)styleos_user_x86_gdt[retained_cs/8u]=styleos_user_x86_gdt[1];
    struct up_tss *t=(struct up_tss *)(void *)styleos_user_x86_tss;
    t->rsp[0]=(uint64_t)(uintptr_t)styleos_user_entry_stack+UP_ENTRY_STACK_BYTES;
    t->iomap=sizeof(*t); /* Outside limit: no user port-I/O bitmap permission. */
    uint64_t base=(uint64_t)(uintptr_t)t;
    styleos_user_x86_gdt[5]=103u|((base&0xffffffu)<<16)|\
        (UINT64_C(0x89)<<40)|((base>>24&255u)<<56);
    styleos_user_x86_gdt[6]=base>>32;
    for(unsigned vector=0;vector<256;++vector){
        uint64_t target=(uint64_t)(uintptr_t)styleos_isr_table[vector];
        if(vector==14u)target=(uint64_t)(uintptr_t)styleos_user_x86_pf;
        if(vector==128u)target=(uint64_t)(uintptr_t)styleos_user_x86_call;
        styleos_user_x86_idt[vector]=(struct up_idt_gate){
            (uint16_t)target,8u,0u,(uint8_t)(vector==128u?0xeeu:0x8eu),
            (uint16_t)(target>>16),(uint32_t)(target>>32),0u};
    }
}

enum up_error up_arch_observe(struct up_machine *out)
{
    if(!out)return UP_ARGUMENT;
    struct descriptor_pointer gdt,idt;
    uint16_t cs,tr;
    __asm__ volatile("sgdt %0":"=m"(gdt));
    __asm__ volatile("sidt %0":"=m"(idt));
    __asm__ volatile("mov %%cs,%0":"=r"(cs));
    __asm__ volatile("str %0":"=r"(tr));
    *out=(struct up_machine){read_cr3(),read_cr0(),read_cr4(),
        read_msr(0xc0000080u),read_msr(0x277u),cs,idt.base,gdt.base,tr};
    return UP_OK;
}

enum up_error up_arch_prepare(const struct boot_memory_evidence *e,struct up_platform *p)
{
    if(!e || !p || !e->map || !inside(e->live.image,e->live.handoff.start,sizeof(struct boot_info)))
        return UP_ARGUMENT;
    const struct boot_info *info=(const struct boot_info *)(uintptr_t)e->live.handoff.start;
    if(info->arch!=STYLEOS_ARCH_X86_64 || info->platform!=STYLEOS_PLATFORM_UEFI ||
       info->flags!=STYLEOS_BOOT_EXITED_SERVICES)return UP_PLATFORM;
    struct observation old={.evidence=e};
    if(!read_state(&old) || !image_identity(&old) || !pe_header(&old) ||
       old.state.idt_bytes!=UP_PAGE || !inside(e->live.image,old.state.idt_base,UP_PAGE) ||
       !admit_new_objects(&old) || !inherited_idt_compatible(&old))return UP_PLATFORM;
    /* This is the first write into arch scratch or any new hardware object. */
    clear_bytes(XCONTROL,sizeof(*XCONTROL));
    clear_bytes(p,sizeof(*p));
#ifdef STYLEOS_TEST_NATIVE_TEA
    /* The inherited VA->PA proof and RESERVED/non-CANDIDATE checks have
     * completed. Only now may the guest copy the verified ELF into BSS. */
    if(!tea_768_prepare())return UP_CONTEXT;
#endif
    if(!build_roots(e,p))return UP_MAPPING;
    build_descriptors((uint16_t)old.state.cs);
    struct up_x86_control *s=XCONTROL;
    s->safe_root=p->roots[0];s->running=UP_SLOTS;
    s->cr0=old.state.mode;s->cr4=old.state.control0;
    s->efer=old.state.control1;s->pat=old.state.pat;s->image=e->live.image;
    const unsigned char *code[2]={styleos_user_code_a,styleos_user_code_b};
    const unsigned char *yield[2]={styleos_user_a_yield,styleos_user_b_yield};
    const unsigned char *resume[2]={styleos_user_a_yield_resume,styleos_user_b_yield_resume};
    for(unsigned id=0;id<2;++id){
        if(yield[id]<code[id] || resume[id]<code[id] ||
           yield[id]>=code[id]+UP_PAGE || resume[id]>=code[id]+UP_PAGE)return UP_CONTEXT;
        p->entry[id]=UP_CODE_VA;
        p->yield_pc[id]=UP_CODE_VA+(uint64_t)(yield[id]-code[id]);
        p->resume_pc[id]=UP_CODE_VA+(uint64_t)(resume[id]-code[id]);
    }
    p->fault_pc[0]=UP_CODE_VA+(uint64_t)(styleos_user_a_fault-styleos_user_code_a);
    p->exit_pc[1]=UP_CODE_VA+(uint64_t)(styleos_user_b_exit-styleos_user_code_b);
#ifdef STYLEOS_TEST_NATIVE_TEA
    p->yield_pc[1]=tea_768_yield_pc();
    p->resume_pc[1]=tea_768_resume_pc();
    p->exit_pc[1]=tea_768_exit_pc();
#endif
    /* All owned destinations and descriptors exist before changing GDTR.
     * IF is0. The tiny ASM interval reloads CS/data/TR and then IDTR under
     * inherited mappings; no fallible firmware operation is called there. */
    const struct descriptor_pointer gdtr={55u,(uint64_t)(uintptr_t)styleos_user_x86_gdt};
    const struct descriptor_pointer transition_gdtr={
        (uint16_t)(old.state.cs>48u?old.state.cs+7u:55u),gdtr.base};
    const struct descriptor_pointer idtr={4095u,(uint64_t)(uintptr_t)styleos_user_x86_idt};
    up_x86_descriptors(&transition_gdtr,&idtr);
    /* New IDT is now live and all its gates use selector8. Narrow away the
     * temporary old-selector alias only after this diagnostic path is valid. */
    __asm__ volatile("lgdt %0"::"m"(gdtr):"memory");
    const uint64_t no_global=s->cr4&~BIT(7),wp=s->cr0|BIT(16);
    __asm__ volatile("mov %0,%%cr4; mov %1,%%cr0; mov %2,%%cr3; mov %3,%%cr4"
        ::"r"(no_global),"r"(wp),"r"(s->safe_root),"r"(s->cr4):"memory");
    if(up_arch_observe(&p->safe)!=UP_OK || p->safe.root!=s->safe_root ||
       p->safe.mode!=wp || p->safe.control0!=s->cr4 || p->safe.control1!=s->efer ||
       p->safe.control2!=s->pat || p->safe.control3!=8u || p->safe.tss!=40u ||
       p->safe.gdt!=gdtr.base || p->safe.vectors!=idtr.base || read_flags()&(BIT(9)|BIT(10)))
        return UP_PLATFORM;
    s->prepared=1;
    up_range_line("OBJECT name=idt",(struct boot_range){old.state.idt_base,old.state.idt_base+UP_PAGE});
    up_range_line("OBJECT name=user_idt",object(styleos_user_x86_idt,UP_PAGE));
    up_range_line("OBJECT name=gdt",object(styleos_user_x86_gdt,UP_PAGE));
    up_range_line("OBJECT name=tss",object(styleos_user_x86_tss,UP_PAGE));
    return UP_OK;
}

/* The C view exactly matches the first176bytes on the dedicated entry stack.
 * CS is checked before reading RSP/SS because a CPL0 fault lacks those words. */
struct x86_raw_frame {
    uint64_t gpr[15],vector,error,pc,cs,status,sp,ss;
};
_Static_assert(sizeof(struct x86_raw_frame)==176,"full user hardware frame");
_Static_assert(offsetof(struct x86_raw_frame,cs)==144,"rawCS offset");

void up_x86_trap(const struct x86_raw_frame *frame,uint64_t cr2,uint64_t root)
{
    if((frame->cs&3u)!=3u){
        if(frame->vector==14u)early_exception_x86(14u,frame->pc,1,frame->error,cr2);
        early_exception_x86(frame->vector,frame->pc,0,0u,0u);
    }
    struct up_x86_control *s=XCONTROL;
    const uint64_t raw=(uint64_t)(uintptr_t)frame;
    const uint64_t top=(uint64_t)(uintptr_t)styleos_user_entry_stack+UP_ENTRY_STACK_BYTES;
    if(!s->prepared || s->running>=UP_SLOTS || !s->context || !s->event ||
       root!=s->selected_root || root!=(uint64_t)(uintptr_t)styleos_user_tables[s->running+1u] ||
       raw<top-UP_ENTRY_STACK_BYTES || raw>top-sizeof(*frame) || (raw&15u) ||
       frame->cs!=0x23u || frame->ss!=0x1bu || frame->status&(BIT(9)|BIT(10)|BIT(12)|BIT(13)))
        up_fail("x86-trap-origin");
    for(unsigned i=0;i<31;++i)s->context->gpr[i]=i<15u?frame->gpr[i]:0u;
    s->context->sp=frame->sp;s->context->pc=frame->pc;s->context->status=frame->status;
    s->context->cs=frame->cs;s->context->ss=frame->ss;
    enum up_kind kind=UP_EVENT_NONE;
    if(frame->vector==14u)kind=UP_FAULT;
    else if(frame->vector==128u && frame->gpr[0]==1u)kind=UP_YIELD;
    else if(frame->vector==128u && frame->gpr[0]==2u)kind=UP_EXIT;
    else up_fail("x86-user-operation");
    *s->event=(struct up_event){kind,frame->vector,frame->error,
        frame->vector==14u?cr2:0u,root,raw};
}

enum up_error up_arch_run(unsigned id,struct up_context *context,struct up_event *event)
{
    struct up_x86_control *s=XCONTROL;
    if(id>=UP_SLOTS || !context || !event)return UP_ARGUMENT;
    if(!s->prepared || s->running!=UP_SLOTS || read_cr3()!=s->safe_root ||
       read_flags()&(BIT(9)|BIT(10)))return UP_STATE;
    const uint64_t stack=id?UP_STACK_B_VA:UP_STACK_A_VA;
    if(context->pc<UP_CODE_VA || context->pc>=UP_CODE_VA+UP_PAGE ||
       context->sp<stack || context->sp>stack+UP_PAGE || (context->sp&15u) ||
       context->cs!=0x23u || context->ss!=0x1bu || context->status&(BIT(9)|BIT(10)|BIT(12)|BIT(13)))
        return UP_CONTEXT;
    clear_bytes(event,sizeof(*event));s->running=id;
    const uint64_t root=(uint64_t)(uintptr_t)styleos_user_tables[id+1u];
    up_x86_enter(context,event,root);
    /* This C continuation runs on exactly the original dispatcher stack;
     * ASM has already loaded SAFE and discarded the privileged entry frame. */
    s->running=UP_SLOTS;
    if(read_cr3()!=s->safe_root || event->kind==UP_EVENT_NONE)return UP_PLATFORM;
    s->context=0;s->event=0;s->selected_root=0;s->dispatcher_sp=0;
    return UP_OK;
}

