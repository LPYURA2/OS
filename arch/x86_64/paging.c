#include "styleos/arch.h"
#include "styleos/paging.h"
#ifdef STYLEOS_TEST_FRAME_GRANTS
#include "styleos/kernel_mapping_inventory.h"
#endif

/* T-P1.4 only: the build does not link this file into any historical profile.
 * Old page-table physical reads use the supported UEFI x64 boot contract's
 * access to physical RAM. They are not a VA-to-PA cast for GDTR or other
 * firmware objects: those translations are resolved through the old CR3.
 * Every table page visited is additionally checked for an identity translation
 * before the result may be activated. Broken inherited mappings are outside
 * the early handler's stack/UART availability guarantee. */
#define BIT(n) (UINT64_C(1) << (n))
#define LOW_VA_LIMIT BIT(47)
#define OLD_TABLE_LIMIT 64u
#define ENTRY_ADDRESS UINT64_C(0x000ffffffffff000)

struct descriptor_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct x86_state {
    uint64_t cr0, cr3, cr4, efer, pat, rflags;
    unsigned bits;
    int nx, one_gib, prepared;
    uint64_t pa_mask;
    struct descriptor_pointer gdtr, idtr;
    uint16_t cs;
    uint64_t code_descriptor;
    uint64_t visited[OLD_TABLE_LIMIT];
    size_t nr_visited;
    uint64_t gdt_va[2], gdt_pa[2];
    size_t gdt_index[2];
    unsigned gdt_flags[2];
    size_t nr_gdt;
};
static struct x86_state saved;

#ifndef STYLEOS_PAGING_X86_HOST
static uint64_t read_cr0(void)
{ uint64_t v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static uint64_t read_cr3(void)
{ uint64_t v; __asm__ volatile("mov %%cr3, %0" : "=r"(v)); return v; }
static uint64_t read_cr4(void)
{ uint64_t v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v; }
static uint64_t read_flags(void)
{ uint64_t v; __asm__ volatile("pushfq; popq %0" : "=r"(v)); return v; }
static uint64_t read_msr(uint32_t msr)
{
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return (uint64_t)low | (uint64_t)high << 32;
}
static void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b,
                  uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                     : "a"(leaf), "c"(0));
}
static void read_descriptors(struct x86_state *s)
{
    __asm__ volatile("sgdt %0" : "=m"(s->gdtr));
    __asm__ volatile("sidt %0" : "=m"(s->idtr));
    __asm__ volatile("mov %%cs, %0" : "=r"(s->cs));
}
static void switch_root(uint64_t root, uint64_t cr0, uint64_t cr4)
{
    /* PCIDE=0 was required. Clearing PGE invalidates global translations;
     * the subsequent ordinary MOV CR3 flushes non-global translations. Own
     * leaves have no global bit. MOV CR3 is serializing; no no-flush operand. */
    const uint64_t no_global = cr4 & ~BIT(7);
    const uint64_t write_protected = cr0 | BIT(16);
    __asm__ volatile("mov %0, %%cr4; mov %1, %%cr0; mov %2, %%cr3; mov %3, %%cr4"
                     :: "r"(no_global), "r"(write_protected), "r"(root),
                        "r"(cr4) : "memory");
}
#else
/* Host tests exercise only the pure mode, descriptor and old-walk checks.
 * These stubs cannot activate a host translation root. */
static uint64_t read_cr0(void) { return 0; }
static uint64_t read_cr3(void) { return 0; }
static uint64_t read_cr4(void) { return 0; }
static uint64_t read_flags(void) { return 0; }
static uint64_t read_msr(uint32_t n) { (void)n; return 0; }
static void cpuid(uint32_t n, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{ (void)n; *a = *b = *c = *d = 0; }
static void read_descriptors(struct x86_state *s) { (void)s; }
static void switch_root(uint64_t r, uint64_t c0, uint64_t c4)
{ (void)r; (void)c0; (void)c4; }
#endif

static int inside(uint64_t start, uint64_t bytes, struct boot_range range)
{
    return bytes && start >= range.start && start < range.end &&
           bytes <= range.end - start;
}

static int mode_ok(const struct x86_state *s)
{
    /* No LA57, PCID, CET/PKS/PKE or SMAP dependency in this narrow profile.
     * SMEP may remain enabled: every own mapping is supervisor-only. */
    const uint64_t unsupported = BIT(12) | BIT(17) | BIT(21) |
                                 BIT(22) | BIT(23) | BIT(24);
    return s->bits >= 32 && s->bits <= 52 && s->nx &&
        (s->cr0 & (BIT(0) | BIT(31))) == (BIT(0) | BIT(31)) &&
        !(s->cr0 & (BIT(29) | BIT(30))) && (s->cr4 & BIT(5)) &&
        !(s->cr4 & unsupported) && !(s->rflags & BIT(9)) &&
        (s->efer & (BIT(8) | BIT(10) | BIT(11))) ==
            (BIT(8) | BIT(10) | BIT(11)) &&
        (s->pat & 255u) == 6u;
}

static int code_descriptor_ok(uint64_t d)
{
    /* Long-mode ring-0 code requires P,S,executable,L=1,D=0. Accessed may be
     * zero in OVMF's descriptor even with the corresponding hidden CS active;
     * that case needs a separately authorized hardware A-bit update page. */
    const unsigned access = (unsigned)(d >> 40) & 255u;
    return (access & 0xf8u) == 0x98u && (access & 0x60u) == 0 &&
           (d & BIT(53)) && !(d & BIT(54));
}

enum walk_status { WALK_OK, WALK_MISSING, WALK_BAD };
struct old_mapping { uint64_t pa; int writable, executable, user; unsigned type; };

static int remember_table(struct x86_state *s, uint64_t page)
{
    if (!page || (page & (PG_PAGE - 1u)) || page >= LOW_VA_LIMIT ||
        (page & ~s->pa_mask)) return 0;
    for (size_t i = 0; i < s->nr_visited; ++i)
        if (s->visited[i] == page) return 1;
    if (s->nr_visited == OLD_TABLE_LIMIT) return 0;
    s->visited[s->nr_visited++] = page;
    return 1;
}

static enum walk_status old_walk(struct x86_state *s, uint64_t va,
                                 struct old_mapping *out)
{
    if (va >= LOW_VA_LIMIT) return WALK_BAD;
    uint64_t page = s->cr3 & ENTRY_ADDRESS;
    int writable = 1, executable = 1, user = 1;
    for (unsigned level = 0; level < 4; ++level) {
        if (!remember_table(s, page)) return WALK_BAD;
        const volatile uint64_t *table = (const volatile uint64_t *)(uintptr_t)page;
        const unsigned shift = 39u - level * 9u;
        uint64_t entry = table[(va >> shift) & 511u];
        if (!(entry & 1u)) return WALK_MISSING;
        if ((entry & ENTRY_ADDRESS & ~s->pa_mask) ||
            (entry & UINT64_C(0x7800000000000000))) return WALK_BAD;
        writable = writable && !!(entry & BIT(1));
        user = user && !!(entry & BIT(2));
        executable = executable && !(entry & BIT(63));
        int large = level < 3 && !!(entry & BIT(7));
        if (large && (level == 0 || (level == 1 && !s->one_gib))) return WALK_BAD;
        if (level == 3 || large) {
            const unsigned leaf_shift = large ? shift : 12u;
            const uint64_t offset_mask = BIT(leaf_shift) - 1u;
            if (large && (entry & ENTRY_ADDRESS & offset_mask & ~BIT(12)))
                return WALK_BAD;
            uint64_t base = entry & s->pa_mask & ~offset_mask;
            unsigned pat_bit = (unsigned)((entry >> (large ? 12u : 7u)) & 1u);
            unsigned index = (unsigned)((entry >> 3) & 3u) | pat_bit << 2;
            out->pa = base | (va & offset_mask);
            out->writable = writable;
            out->user = user;
            out->executable = executable;
            out->type = (unsigned)((s->pat >> (index * 8u)) & 255u);
            return WALK_OK;
        }
        /* Narrow supported profile: old hardware walks and our physical
         * table-page reads both use WB, without PWT/PCD cache overrides. */
        if (entry & UINT64_C(0x18)) return WALK_BAD;
        page = entry & s->pa_mask & ENTRY_ADDRESS;
    }
    return WALK_BAD;
}

static int identity_tables(struct x86_state *s)
{
    /* Verification may discover more old tables; bounded closure includes them
     * all. A table PA must itself have the identity access used by this walker. */
    for (size_t i = 0; i < s->nr_visited; ++i) {
        struct old_mapping mapping;
        if (old_walk(s, s->visited[i], &mapping) != WALK_OK ||
            mapping.pa != s->visited[i] || mapping.type != 6u) return 0;
    }
    return 1;
}

static int image_identity(struct x86_state *s, struct boot_range image)
{
    if (image.start >= image.end || (image.start & (PG_PAGE - 1u)) ||
        (image.end & (PG_PAGE - 1u)) || image.end > LOW_VA_LIMIT) return 0;
    for (uint64_t va = image.start; va < image.end; va += PG_PAGE) {
        struct old_mapping m;
        if (old_walk(s, va, &m) != WALK_OK || m.pa != va || m.type != 6u)
            return 0;
    }
    return 1;
}

static int writable_identity(struct x86_state *s, uint64_t va, uint64_t bytes,
                              struct boot_range image)
{
    if ((va | bytes) & (PG_PAGE - 1u) || !inside(va, bytes, image)) return 0;
    for (uint64_t at = va; at < va + bytes; at += PG_PAGE) {
        struct old_mapping m;
        if (old_walk(s, at, &m) != WALK_OK || m.pa != at ||
            !m.writable || m.type != 6u) return 0;
    }
    return 1;
}

static uint64_t load64(const volatile unsigned char *p)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= (uint64_t)p[i] << (8u * i);
    return value;
}

static int descriptor_bounds_ok(const struct x86_state *s, struct boot_range image)
{
    if (!s->cs || (s->cs & 7u) || s->gdtr.limit > 4095u ||
        s->cs > s->gdtr.limit || 7u > (unsigned)(s->gdtr.limit - s->cs) ||
        (s->gdtr.base & 7u) || s->gdtr.base >= LOW_VA_LIMIT ||
        (uint64_t)s->gdtr.limit + 1u > LOW_VA_LIMIT - s->gdtr.base ||
        s->idtr.limit != 4095u || !inside(s->idtr.base, 4096u, image)) return 0;
    return 1;
}

static int descriptors_ok(struct x86_state *s, struct boot_range image)
{
    if (!descriptor_bounds_ok(s, image)) return 0;
    s->code_descriptor = load64((const volatile unsigned char *)(uintptr_t)
                               (s->gdtr.base + s->cs));
    if (!code_descriptor_ok(s->code_descriptor)) {
        arch_puts("STYLEOS PAGING X86 BAD CS descriptor=");
        paging_hex(s->code_descriptor); arch_putc('\n');
        return 0;
    }
    const volatile unsigned char *gates =
        (const volatile unsigned char *)(uintptr_t)s->idtr.base;
    for (unsigned i = 0; i < 256; ++i) {
        const volatile unsigned char *p = gates + i * 16u;
        uint64_t low = load64(p), high = load64(p + 8);
        uint16_t selector = (uint16_t)(low >> 16);
        uint64_t target = (low & 65535u) | ((low >> 48) << 16) | (high << 32);
        if (selector != s->cs || ((low >> 32) & 255u) ||
            ((low >> 40) & 255u) != 0x8eu || high >> 32 ||
            !inside(target, 1, image)) {
            arch_puts("STYLEOS PAGING X86 BAD IDT index="); paging_hex(i);
            arch_puts(" low="); paging_hex(low);
            arch_puts(" high="); paging_hex(high); arch_putc('\n');
            return 0;
        }
    }
    return 1;
}

static void field(const char *name, uint64_t value)
{
    arch_puts(name); paging_hex(value);
}

static enum pg_error refuse(const char *stage)
{
    arch_puts("STYLEOS PAGING X86 REFUSE ");
    arch_puts(stage); arch_putc('\n');
    return PG_PLATFORM;
}

static int hardware_gdt_write_allowed(const struct boot_memory_evidence *e,
                                      uint64_t page)
{
    if (!e || !e->map || (page & (PG_PAGE - 1u)) || page > UINT64_MAX-PG_PAGE ||
        e->map->nr_reserved > BOOT_MEMORY_INPUT_MAX ||
        e->map->nr_candidates > BOOT_MEMORY_RANGE_MAX) return 0;
    if ((page < e->live.image.end && e->live.image.start < page+PG_PAGE) ||
        (page < e->live.source.end && e->live.source.start < page+PG_PAGE)) return 0;
    int reserved = 0;
    for (size_t i = 0; i < e->map->nr_reserved; ++i) {
        struct boot_range r = e->map->reserved[i];
        if (r.start >= r.end) return 0;
        if (r.start <= page && page + PG_PAGE <= r.end) reserved = 1;
    }
    if (!reserved) return 0;
    for (size_t i = 0; i < e->map->nr_candidates; ++i) {
        struct boot_range r = e->map->candidates[i];
        if (r.start >= r.end || (page < r.end && r.start < page+PG_PAGE)) return 0;
    }
    return 1;
}

enum pg_error arch_paging_prepare(struct pg_experiment *x)
{
    if (!x || !x->evidence || x->plan.arch != PG_X86) return PG_ARGUMENT;
    struct x86_state *s = &saved;
    *s = (struct x86_state){0};
    uint32_t a, b, c, d;
    cpuid(0x80000000u, &a, &b, &c, &d);
    if (a < 0x80000008u) return refuse("cpuid");
    cpuid(0x80000001u, &a, &b, &c, &d);
    s->nx = !!(d & (1u << 20)); s->one_gib = !!(d & (1u << 26));
    cpuid(0x80000008u, &a, &b, &c, &d);
    s->bits = a & 255u;
    s->cr0 = read_cr0(); s->cr3 = read_cr3(); s->cr4 = read_cr4();
    s->efer = read_msr(0xc0000080u); s->pat = read_msr(0x277u);
    s->rflags = read_flags();
    arch_puts("STYLEOS PAGING X86 BEFORE");
    field(" cr0=", s->cr0); field(" cr3=", s->cr3); field(" cr4=", s->cr4);
    field(" efer=", s->efer); field(" pat=", s->pat);
    field(" rflags=", s->rflags); field(" bits=", s->bits);
    arch_putc('\n');
    if (!mode_ok(s)) return refuse("mode");
    s->pa_mask = (BIT(s->bits) - 1u) & ENTRY_ADDRESS;
    if (s->cr3 & ~s->pa_mask) return refuse("old-root");
    x->plan.pa_bits = s->bits;
    if (!image_identity(s, x->evidence->live.image)) return refuse("image-identity");
    struct old_mapping m;
    if (old_walk(s, PG_ALIAS, &m) != WALK_MISSING ||
        old_walk(s, PG_PROBE, &m) != WALK_MISSING) return PG_PROBE_MAPPED;
    read_descriptors(s);
    if (!descriptor_bounds_ok(s, x->evidence->live.image)) return refuse("descriptor-bounds");
    uint64_t start = s->gdtr.base & ~(uint64_t)(PG_PAGE - 1u);
    uint64_t end = (s->gdtr.base + s->gdtr.limit + PG_PAGE) &
                   ~(uint64_t)(PG_PAGE - 1u);
    for (uint64_t va = start; va < end; va += PG_PAGE) {
        if (s->nr_gdt == 2 || x->plan.count >= PG_MAX_MAPS) return PG_CAPACITY;
        if (old_walk(s, va, &m) != WALK_OK || m.type != 6u) return refuse("gdt-walk");
        /* Do not create a RO alias of image data, nor broaden an existing VA
         * mapping to accommodate firmware. The generic builder also rejects
         * any physical alias with conflicting write/type policy. */
        struct boot_range image = x->evidence->live.image;
        if ((va < image.end && image.start < va + PG_PAGE) ||
            (m.pa < image.end && image.start < m.pa + PG_PAGE)) return PG_CONFLICT;
        s->gdt_va[s->nr_gdt] = va; s->gdt_pa[s->nr_gdt] = m.pa;
        s->gdt_index[s->nr_gdt++] = x->plan.count;
        x->plan.maps[x->plan.count++] = (struct pg_mapping){va, m.pa, PG_PAGE, 0};
    }
    if (!identity_tables(s)) return refuse("old-table-identity");
    /* Numeric descriptor bounds, every GDT VA translation and the identity
     * access used by the walker were proved before dereferencing GDT bytes. */
    if (!descriptors_ok(s, x->evidence->live.image)) return refuse("descriptor-content");
    if (!(s->code_descriptor & BIT(40))) {
        /* No software store to firmware GDT. The first exception may cause
         * hardware to mark CS Accessed. Only that descriptor's page is made
         * writable/NX, after proving its complete PA page already reserved
         * and outside diagnostic candidates. Other GDT pages remain RO/NX. */
        uint64_t accessed_page = (s->gdtr.base+s->cs+5u) & ~(uint64_t)(PG_PAGE-1u);
        int found = 0;
        for (size_t i = 0; i < s->nr_gdt; ++i) {
            if (s->gdt_va[i] != accessed_page) continue;
            if (!hardware_gdt_write_allowed(x->evidence, s->gdt_pa[i]))
                return refuse("gdt-accessed-page-not-reserved");
            s->gdt_flags[i] = PG_WRITE;
            x->plan.maps[s->gdt_index[i]].flags = PG_WRITE;
            found = 1;
        }
        if (!found) return refuse("gdt-accessed-page-missing");
    }
    /* Identity was checked for every image page. These are translations of
     * known loaded-image objects, not physical values inferred by a cast. */
    uint64_t tables_va = (uint64_t)(uintptr_t)styleos_paging_tables;
    uint64_t control_va = (uint64_t)(uintptr_t)&styleos_paging_control;
    uint64_t data_va = (uint64_t)(uintptr_t)styleos_paging_data;
    if (!writable_identity(s, tables_va, PG_TABLE_BYTES, x->evidence->live.image) ||
        !writable_identity(s, control_va, PG_CONTROL_BYTES, x->evidence->live.image) ||
        !writable_identity(s, data_va, PG_DATA_BYTES, x->evidence->live.image) ||
        !identity_tables(s)) return refuse("storage-writable");
    if (old_walk(s, tables_va, &m) != WALK_OK || m.pa != tables_va || !m.writable)
        return refuse("tables-translation");
    x->plan.tables_pa = m.pa;
    if (old_walk(s, data_va, &m) != WALK_OK || m.pa != data_va || !m.writable)
        return refuse("data-translation");
    x->data_pa = m.pa;
    x->old_root = s->cr3 & s->pa_mask; x->before_mode = s->cr0;
    if (old_walk(s, s->gdtr.base, &m) != WALK_OK) return refuse("gdt-first-byte");
    arch_puts("STYLEOS PAGING X86 GDT");
    field(" va=", s->gdtr.base); field(" pa=", m.pa);
    field(" bytes=", (uint64_t)s->gdtr.limit + 1u); field(" cs=", s->cs);
    field(" descriptor=", s->code_descriptor);
    arch_putc('\n');
    arch_puts("STYLEOS PAGING X86 IDT");
    field(" va=", s->idtr.base); field(" bytes=", (uint64_t)s->idtr.limit + 1u);
    arch_putc('\n');
    s->prepared = 1;
    return PG_OK;
}

enum pg_error arch_paging_activate(struct pg_experiment *x)
{
    struct x86_state *s = &saved;
    if (!x || !s->prepared || x->result.error != PG_OK || !x->result.used ||
        x->result.used > x->plan.capacity ||
        x->result.root != x->plan.tables_pa || !x->result.root ||
        (x->result.root & (PG_PAGE - 1u)) ||
        read_cr3() != s->cr3 || read_cr0() != s->cr0 || read_cr4() != s->cr4 ||
        read_msr(0xc0000080u) != s->efer || read_msr(0x277u) != s->pat ||
        read_flags() & BIT(9)) return PG_PLATFORM;
    for (size_t i = 0; i < s->nr_gdt; ++i) {
        struct pg_lookup m;
        if (pg_lookup(&x->plan, s->gdt_va[i], &m) != PG_OK ||
            m.pa != s->gdt_pa[i] || m.flags != s->gdt_flags[i]) return PG_CONTEXT;
    }
    /* Validate both bounds of every IDT/stack page before the one-way switch.
     * Full image mapping also covers code, continuation, vectors and tables. */
    for (uint64_t va = x->evidence->live.image.start;
         va < x->evidence->live.image.end; va += PG_PAGE) {
        struct pg_lookup m;
        if (pg_lookup(&x->plan, va, &m) != PG_OK || m.pa != va ||
            m.flags != (PG_WRITE | PG_EXEC)) return PG_CONTEXT;
    }
    s->prepared = 0; /* One-way experiment, not a reusable activation API. */
    switch_root(x->result.root, s->cr0, s->cr4);
    const uint64_t cr0 = read_cr0(), cr3 = read_cr3(), cr4 = read_cr4();
    const uint64_t efer = read_msr(0xc0000080u);
    if (cr3 != x->result.root || cr0 != (s->cr0 | BIT(16)) ||
        cr4 != s->cr4 || efer != s->efer) return PG_PLATFORM;
    struct x86_state descriptors = {0};
    read_descriptors(&descriptors);
    if (descriptors.gdtr.base != s->gdtr.base || descriptors.gdtr.limit != s->gdtr.limit ||
        descriptors.idtr.base != s->idtr.base || descriptors.idtr.limit != s->idtr.limit ||
        descriptors.cs != s->cs ||
        load64((const volatile unsigned char *)(uintptr_t)(s->gdtr.base + s->cs)) !=
            s->code_descriptor) return PG_PLATFORM;
    x->after_mode = cr0;
    arch_puts("STYLEOS PAGING X86 AFTER");
    field(" cr0=", cr0); field(" cr3=", cr3); field(" cr4=", cr4); field(" efer=", efer);
    arch_putc('\n');
    return PG_OK;
}

#ifdef STYLEOS_TEST_FRAME_GRANTS
/* Read-only hardware inventory on the *restored inherited* CR3. This walks
 * every page of the image and active boot stack, plus the actual GDTR/IDTR
 * ranges and every old page-table frame touched by the walker. TSS, IST,
 * firmware-runtime and device/MMIO dependencies remain explicitly unknown.
 * This MUST NOT authorize switching a detached CR3. */
static int inherited_record(struct x86_state *state, struct kmi_inventory *report,
                             enum kmi_kind kind, uint64_t va, int identity)
{
    struct old_mapping m;
    if (old_walk(state, va, &m) != WALK_OK) return 0;
    const struct kmi_observation o={va,m.pa,m.type,
        !!m.writable,!!m.executable,!!m.user};
    return kmi_account(report,kind,&o,identity)!=KMI_OK ? 0 : 1;
}
static int inherited_range(struct x86_state *state, struct kmi_inventory *report,
                            enum kmi_kind kind, uint64_t start,uint64_t end,
                            int identity)
{
    if (!start || start>=end || end>LOW_VA_LIMIT ||
        end > UINT64_MAX-(PG_PAGE-1u)) return 0;
    const uint64_t first=start&~(uint64_t)(PG_PAGE-1u);
    const uint64_t last=(end+PG_PAGE-1u)&~(uint64_t)(PG_PAGE-1u);
    for (uint64_t va=first; va<last; va+=PG_PAGE)
        if (!inherited_record(state,report,kind,va,identity)) return 0;
    return 1;
}
enum pg_error arch_paging_inventory_inherited(struct pg_experiment *x)
{
    struct x86_state *s=&saved;
    if (!x || !x->evidence || !x->old_root || x->old_root!=s->cr3 ||
        s->prepared || x->result.error!=PG_OK || x->result.root==s->cr3 ||
        read_cr3()!=s->cr3 || read_cr0()!=(s->cr0|BIT(16)) ||
        read_cr4()!=s->cr4 || read_msr(0xc0000080u)!=s->efer ||
        read_msr(0x277u)!=s->pat || read_flags()&BIT(9)) return PG_PLATFORM;
    struct x86_state now={0}; read_descriptors(&now);
    if(now.gdtr.base!=s->gdtr.base || now.gdtr.limit!=s->gdtr.limit ||
       now.idtr.base!=s->idtr.base || now.idtr.limit!=s->idtr.limit ||
       now.cs!=s->cs) return PG_PLATFORM;
    struct kmi_inventory report={0};
    if(!inherited_range(s,&report,KMI_IMAGE,x->evidence->live.image.start,
                         x->evidence->live.image.end,1) ||
       !inherited_range(s,&report,KMI_BOOT_STACK,x->evidence->live.stack.start,
                         x->evidence->live.stack.end,0) ||
       !inherited_range(s,&report,KMI_GDT,s->gdtr.base,
                         s->gdtr.base+(uint64_t)s->gdtr.limit+1u,0) ||
       !inherited_range(s,&report,KMI_IDT,s->idtr.base,
                         s->idtr.base+(uint64_t)s->idtr.limit+1u,0))
        return PG_CONTEXT;
    /* old_walk may have visited additional pages while inventorying. */
    for(size_t i=0;i<s->nr_visited;++i)
        if(!inherited_record(s,&report,KMI_OLD_TABLE,s->visited[i],1))
            return PG_CONTEXT;
    if(!report.pages[KMI_IMAGE] || !report.pages[KMI_BOOT_STACK] ||
       !report.pages[KMI_GDT] || !report.pages[KMI_IDT] ||
       !report.pages[KMI_OLD_TABLE] || !report.rwx_pages ||
       report.root_activation_permitted || !identity_tables(s) ||
       read_cr3()!=s->cr3) return PG_CONTEXT;
    uint16_t tr=0;
#ifndef STYLEOS_PAGING_X86_HOST
    __asm__ volatile("str %0" : "=r"(tr));
#endif
    /* All counts below come from live old_walk() PTE translations. RWX is
     * legacy UEFI image policy, recorded as a blocker, NOT accepted as W^X.
     * TR is observational only: a selector does not prove a TSS/IST mapping.
     * No device memory is touched; I/O-port UART does not prove GOP/MMIO. */
    arch_puts("STYLEOS KMI 779 INHERITED");
    field(" cr3=",s->cr3);
    field(" image=",report.pages[KMI_IMAGE]);
    field(" stack=",report.pages[KMI_BOOT_STACK]);
    field(" gdt=",report.pages[KMI_GDT]);
    field(" idt=",report.pages[KMI_IDT]);
    field(" tables=",report.pages[KMI_OLD_TABLE]);
    field(" rwx=",report.rwx_pages);
    field(" tr=",tr);
    arch_puts(" tss-ist=unverified devices=unverified wx=unverified active=0\n");
    return PG_OK;
}
/* Only while the inherited root is actually active: the old page-table
 * walker assumes firmware's identity physical RAM reads are valid. */
enum pg_error arch_paging_attest_image_writable(struct pg_experiment *x,
                                                uint64_t va, uint64_t bytes)
{
    struct x86_state *s=&saved;
    /* arch_paging_activate deliberately consumed 'prepared'.  Here we
     * require that activation finished AND the inherited root has really
     * been restored; accepting a stale before-activation flag is unsafe. */
    if(!x || s->prepared || !x->evidence || !bytes ||
       x->result.error != PG_OK || !x->result.root ||
       x->result.root == s->cr3 || x->old_root != s->cr3 ||
       (va | bytes) & (PG_PAGE-1u) ||
       read_cr3()!=s->cr3 || read_cr0()!=(s->cr0 | BIT(16)) ||
       read_cr4()!=s->cr4 || read_msr(0xc0000080u)!=s->efer ||
       read_msr(0x277u)!=s->pat || read_flags() & BIT(9) ||
       !writable_identity(s,va,bytes,x->evidence->live.image) ||
       !identity_tables(s)) return PG_PLATFORM;
    return PG_OK;
}
#endif

#if defined(STYLEOS_TEST_PAGING_ROUNDTRIP) || defined(STYLEOS_TEST_FRAME_GRANTS)
/* A second, guarded hardware transition, available ONLY to the dedicated
 * experiment. The original root remains valid because the test does not
 * reclaim firmware pages or allocate from candidate RAM. Both roots retain
 * the image, stack, descriptor tables and interrupt-disabled continuity. */
enum pg_error arch_paging_restore(struct pg_experiment *x)
{
    struct x86_state *s = &saved;
    if (!x || s->prepared || !x->old_root || x->old_root != s->cr3 ||
        x->result.error != PG_OK || x->result.root == s->cr3 ||
        read_cr3() != x->result.root || read_cr0() != (s->cr0 | BIT(16)) ||
        read_cr4() != s->cr4 || read_msr(0xc0000080u) != s->efer ||
        read_msr(0x277u) != s->pat || read_flags() & BIT(9))
        return PG_PLATFORM;
    const uint64_t candidate = read_cr3();
    switch_root(s->cr3, s->cr0 | BIT(16), s->cr4);
    const uint64_t restored = read_cr3(), cr0 = read_cr0();
    const uint64_t cr4 = read_cr4(), efer = read_msr(0xc0000080u);
    if (restored != s->cr3 || cr0 != (s->cr0 | BIT(16)) ||
        cr4 != s->cr4 || efer != s->efer || read_flags() & BIT(9) ||
        !image_identity(s, x->evidence->live.image) ||
        !identity_tables(s)) return PG_PLATFORM;
    struct x86_state descriptors = {0};
    read_descriptors(&descriptors);
    if (descriptors.gdtr.base != s->gdtr.base ||
        descriptors.gdtr.limit != s->gdtr.limit ||
        descriptors.idtr.base != s->idtr.base ||
        descriptors.idtr.limit != s->idtr.limit ||
        descriptors.cs != s->cs) return PG_PLATFORM;
    arch_puts("STYLEOS PAGING X86 RETURN");
    field(" before=", s->cr3); field(" candidate=", candidate);
    field(" after=", restored); field(" cr0=", cr0);
    field(" cr4=", cr4); field(" efer=", efer);
    arch_putc('\n');
    return PG_OK;
}
#endif
