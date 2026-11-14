#include "styleos/ownership.h"

/* This adapter runs only on the inherited UEFI/OVMF CR3, after handoff,
 * MEMMAP and admission of the image-owned ownership workspace. It never
 * changes translation, descriptors or interrupt state. Physical table reads
 * use the same narrow UEFI x64 physical-access contract as ADR-0007, with an
 * additional admission check BEFORE each read: the whole table page must
 * already be baseline RESERVED and outside CANDIDATE. Only the actual CR3 and
 * children reached from it may be read; ordinary RAM is never probed.
 *
 * A bounded identity closure subsequently verifies the access used for every
 * visited table page. This is a proof of that partial set, never a traversal
 * or completeness claim for all inherited tables or other firmware state. */
#define BIT(n) (UINT64_C(1) << (n))
#define LOW_VA_LIMIT BIT(47)
#define ENTRY_ADDRESS UINT64_C(0x000ffffffffff000)

struct descriptor_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct x86_state {
    uint64_t cr0, cr3, cr4, efer, pat, rflags;
    unsigned bits;
    int nx, one_gib;
    uint64_t pa_mask;
    struct descriptor_pointer gdtr, idtr;
    uint16_t cs;
    uint64_t visited[OP_TABLES_MAX];
    size_t nr_visited;
    const struct op_input *input;
};

#ifndef STYLEOS_OWNERSHIP_X86_HOST
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
#else
/* The pure helper tests can run on native ARM64/Termux. These stubs refuse
 * collection, contain no privileged instructions and cannot activate a root. */
static uint64_t read_cr0(void) { return 0; }
static uint64_t read_cr3(void) { return 0; }
static uint64_t read_cr4(void) { return 0; }
static uint64_t read_flags(void) { return 0; }
static uint64_t read_msr(uint32_t n) { (void)n; return 0; }
static void cpuid(uint32_t n, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{ (void)n; *a = *b = *c = *d = 0; }
static void read_descriptors(struct x86_state *s) { (void)s; }
#endif

static int inside(uint64_t start, uint64_t bytes, struct boot_range range)
{
    return bytes && start >= range.start && start < range.end &&
           bytes <= range.end - start;
}

static int mode_ok(const struct x86_state *s)
{
    /* Four levels, no PCID or dependencies on SMAP/PKE/CET/PKS. PAT WB is
     * the supported inherited RAM profile, not a general MTRR proof. */
    const uint64_t unsupported = BIT(12) | BIT(17) | BIT(21) |
                                 BIT(22) | BIT(23) | BIT(24);
    return s->bits >= 32 && s->bits <= 52 && s->nx &&
        (s->cr0 & (BIT(0) | BIT(31))) == (BIT(0) | BIT(31)) &&
        !(s->cr0 & (BIT(29) | BIT(30))) && (s->cr4 & BIT(5)) &&
        !(s->cr4 & unsupported) && !(s->rflags & (BIT(9) | BIT(10))) &&
        (s->efer & (BIT(8) | BIT(10) | BIT(11))) ==
            (BIT(8) | BIT(10) | BIT(11)) && (s->pat & 255u) == 6u;
}

static int read_state(struct x86_state *s)
{
    uint32_t a, b, c, d;
    cpuid(0x80000000u, &a, &b, &c, &d);
    if (a < 0x80000008u) return 0;
    cpuid(0x80000001u, &a, &b, &c, &d);
    s->nx = !!(d & (1u << 20)); s->one_gib = !!(d & (1u << 26));
    cpuid(0x80000008u, &a, &b, &c, &d);
    s->bits = a & 255u;
    s->cr0 = read_cr0(); s->cr3 = read_cr3(); s->cr4 = read_cr4();
    s->efer = read_msr(0xc0000080u); s->pat = read_msr(0x277u);
    s->rflags = read_flags();
    read_descriptors(s);
    if (!mode_ok(s)) return 0;
    s->pa_mask = (BIT(s->bits) - 1u) & ENTRY_ADDRESS;
    /* No PCID, cache override or no-flush bits: raw CR3 equals its PA. */
    return s->cr3 && !(s->cr3 & ~s->pa_mask) && s->cr3 < LOW_VA_LIMIT;
}

static int reserved_page(const struct op_input *input, uint64_t page)
{
    if (!input || !input->map || (page & (OP_PAGE - 1u)) ||
        page > UINT64_MAX - OP_PAGE) return 0;
    const struct boot_memory_map *map = input->map;
    if (map->nr_reserved > BOOT_MEMORY_INPUT_MAX ||
        map->nr_candidates > BOOT_MEMORY_RANGE_MAX) return 0;
    int reserved = 0;
    for (size_t i = 0; i < map->nr_reserved; ++i) {
        struct boot_range r = map->reserved[i];
        if (r.start >= r.end) return 0;
        if (r.start <= page && page + OP_PAGE <= r.end) reserved = 1;
    }
    if (!reserved) return 0;
    for (size_t i = 0; i < map->nr_candidates; ++i) {
        struct boot_range r = map->candidates[i];
        if (r.start >= r.end || (page < r.end && r.start < page + OP_PAGE))
            return 0;
    }
    return 1;
}

enum walk_status { WALK_OK, WALK_MISSING, WALK_BAD };
struct old_mapping { uint64_t pa; int writable, executable; unsigned type; };

static int remember_table(struct x86_state *s, uint64_t page)
{
    if (!page || (page & (OP_PAGE - 1u)) || page >= LOW_VA_LIMIT ||
        (page & ~s->pa_mask) || !reserved_page(s->input, page)) return 0;
    for (size_t i = 0; i < s->nr_visited; ++i)
        if (s->visited[i] == page) return 1;
    if (s->nr_visited >= OP_TABLES_MAX) return 0;
    s->visited[s->nr_visited++] = page;
    return 1;
}

static enum walk_status old_walk_record(struct x86_state *s, uint64_t va,
                                        struct old_mapping *out,
                                        struct op_walk *record)
{
    if (record) *record = (struct op_walk){.va = va};
    if (va >= LOW_VA_LIMIT || !s->cr3 || (s->cr3 & ~s->pa_mask))
        return WALK_BAD;
    uint64_t page = s->cr3;
    int writable = 1, executable = 1;
    for (unsigned level = 0; level < 4; ++level) {
        /* Admission precedes the physical read, including for the root.
         * Subsequent pages come only from this active path's descriptors. */
        if (!remember_table(s, page)) return WALK_BAD;
        const volatile uint64_t *table = (const volatile uint64_t *)(uintptr_t)page;
        const unsigned shift = 39u - level * 9u;
        const uint64_t entry = table[(va >> shift) & 511u];
        if (record) {
            record->tables[level] = page; record->entries[level] = entry;
            record->levels = level + 1u;
        }
        if (!(entry & 1u)) return WALK_MISSING;
        if ((entry & ENTRY_ADDRESS & ~s->pa_mask) ||
            (entry & UINT64_C(0x7800000000000000))) return WALK_BAD;
        writable = writable && !!(entry & BIT(1));
        executable = executable && !(entry & BIT(63));
        const int large = level < 3 && !!(entry & BIT(7));
        if (large && (level == 0 || (level == 1 && !s->one_gib)))
            return WALK_BAD;
        if (level == 3 || large) {
            const unsigned leaf_shift = large ? shift : 12u;
            const uint64_t offset_mask = BIT(leaf_shift) - 1u;
            if (large && (entry & ENTRY_ADDRESS & offset_mask & ~BIT(12)))
                return WALK_BAD;
            const uint64_t base = entry & s->pa_mask & ~offset_mask;
            const unsigned pat_bit = (unsigned)((entry >> (large ? 12u : 7u)) & 1u);
            const unsigned index = (unsigned)((entry >> 3) & 3u) | pat_bit << 2;
            out->pa = base | (va & offset_mask);
            out->writable = writable; out->executable = executable;
            out->type = (unsigned)((s->pat >> (index * 8u)) & 255u);
            if (record) record->pa = out->pa;
            return WALK_OK;
        }
        if (entry & UINT64_C(0x18)) return WALK_BAD;
        page = entry & s->pa_mask & ENTRY_ADDRESS;
    }
    return WALK_BAD;
}

static enum walk_status old_walk(struct x86_state *s, uint64_t va,
                                 struct old_mapping *out)
{
    return old_walk_record(s, va, out, 0);
}

static int identity_tables(struct x86_state *s)
{
    /* Walking table identities may discover more active child pages. The
     * limit applies to the entire closure, rather than each individual walk. */
    for (size_t i = 0; i < s->nr_visited; ++i) {
        struct old_mapping mapping;
        if (old_walk(s, s->visited[i], &mapping) != WALK_OK ||
            mapping.pa != s->visited[i] || mapping.type != 6u) return 0;
    }
    return 1;
}

static int record_identity_tables(struct x86_state *s, struct op_input *input)
{
    /* Preserve one actual descriptor path per known table identity. New pages
     * discovered while proving those identities join this same bounded loop. */
    for (size_t i = 0; i < s->nr_visited; ++i) {
        if (input->nr_walks >= OP_WALKS_MAX) return 0;
        struct old_mapping mapping;
        struct op_walk *record = &input->walks[input->nr_walks];
        if (old_walk_record(s, s->visited[i], &mapping, record) != WALK_OK ||
            mapping.pa != s->visited[i] || mapping.type != 6u) return 0;
        record->kind = OP_WALK_TABLE;
        ++input->nr_walks;
    }
    return 1;
}

static int image_identity(struct x86_state *s, struct boot_range image)
{
    if (image.start >= image.end || (image.start & (OP_PAGE - 1u)) ||
        (image.end & (OP_PAGE - 1u)) || image.end > LOW_VA_LIMIT) return 0;
    for (uint64_t va = image.start; va < image.end; va += OP_PAGE) {
        struct old_mapping mapping;
        if (!reserved_page(s->input, va) || old_walk(s, va, &mapping) != WALK_OK ||
            mapping.pa != va || mapping.type != 6u) return 0;
    }
    return 1;
}

static uint64_t load64(const volatile unsigned char *p)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= (uint64_t)p[i] << (8u * i);
    return value;
}

static int code_descriptor_ok(uint64_t descriptor)
{
    /* Accept A=0 as well: this profile does not change any GDT mapping or
     * software descriptor byte, and retains the inherited hardware policy. */
    const unsigned access = (unsigned)(descriptor >> 40) & 255u;
    return (access & 0xf8u) == 0x98u && (access & 0x60u) == 0 &&
           (descriptor & BIT(53)) && !(descriptor & BIT(54));
}

static int descriptor_bounds_ok(const struct x86_state *s, struct boot_range image)
{
    return s->cs && !(s->cs & 7u) && s->gdtr.limit <= 4095u &&
        s->cs <= s->gdtr.limit && 7u <= (unsigned)(s->gdtr.limit - s->cs) &&
        !(s->gdtr.base & 7u) && s->gdtr.base < LOW_VA_LIMIT &&
        (uint64_t)s->gdtr.limit + 1u <= LOW_VA_LIMIT - s->gdtr.base &&
        s->idtr.limit == 4095u && inside(s->idtr.base, 4096u, image);
}

static int descriptors_ok(struct x86_state *s, struct boot_range image)
{
    /* Caller already proved full image identity, GDT VA translations and all
     * physical table accesses. No arbitrary GDT VA is treated as a PA. */
    if (!descriptor_bounds_ok(s, image) ||
        !code_descriptor_ok(load64((const volatile unsigned char *)(uintptr_t)
                                  (s->gdtr.base + s->cs)))) return 0;
    const volatile unsigned char *gates =
        (const volatile unsigned char *)(uintptr_t)s->idtr.base;
    for (unsigned i = 0; i < 256; ++i) {
        const volatile unsigned char *p = gates + i * 16u;
        const uint64_t low = load64(p), high = load64(p + 8);
        const uint16_t selector = (uint16_t)(low >> 16);
        const uint64_t target = (low & 65535u) | ((low >> 48) << 16) | (high << 32);
        struct old_mapping mapping;
        if (selector != s->cs || ((low >> 32) & 255u) ||
            ((low >> 40) & 255u) != 0x8eu || high >> 32 ||
            !inside(target, 1u, image) || old_walk(s, target, &mapping) != WALK_OK ||
            mapping.pa != target || mapping.type != 6u || !mapping.executable)
            return 0;
    }
    return 1;
}

static struct op_snapshot snapshot_state(const struct x86_state *state)
{
    return (struct op_snapshot){
        .mode = state->cr0, .root = state->cr3, .aux0 = state->cr4, .aux1 = state->efer,
        .pat = state->pat, .rflags = state->rflags,
        .gdt_va = state->gdtr.base, .gdt_bytes = (uint64_t)state->gdtr.limit + 1u,
        .idt_va = state->idtr.base, .idt_bytes = (uint64_t)state->idtr.limit + 1u,
        .cs = state->cs, .pa_bits = state->bits, .phase = OP_INHERITED,
        .features = (unsigned)state->nx | (unsigned)state->one_gib << 1};
}

int op_arch_unchanged(const struct op_input *input, struct op_snapshot *after)
{
    if (!after) return 0;
    *after = (struct op_snapshot){0};
    if (!input || input->arch != STYLEOS_ARCH_X86_64 || input->source != OP_UEFI ||
        input->snapshot.phase != OP_INHERITED) return 0;
    struct x86_state current = {0};
    if (!read_state(&current)) return 0;
    *after = snapshot_state(&current);
    const struct op_snapshot *saved = &input->snapshot;
    /* Arithmetic flags naturally change across C calls. IF and DF are the
     * relevant inherited execution contract and must both stay masked/clear. */
    return current.cr0 == saved->mode && current.cr3 == saved->root &&
        current.cr4 == saved->aux0 && current.efer == saved->aux1 &&
        current.pat == saved->pat &&
        !((current.rflags ^ saved->rflags) & (BIT(9) | BIT(10))) &&
        current.bits == saved->pa_bits &&
        ((unsigned)current.nx | (unsigned)current.one_gib << 1) == saved->features &&
        current.gdtr.base == saved->gdt_va &&
        (uint64_t)current.gdtr.limit + 1u == saved->gdt_bytes &&
        current.idtr.base == saved->idt_va &&
        (uint64_t)current.idtr.limit + 1u == saved->idt_bytes && current.cs == saved->cs;
}

enum op_error op_arch_collect(struct op_input *input, const struct boot_info *info)
{
    if (!input || !info || !input->map) return OP_ARGUMENT;
    if (input->nr_walks) return OP_CONTEXT;
    if (input->arch != STYLEOS_ARCH_X86_64 || input->source != OP_UEFI ||
        info->arch != STYLEOS_ARCH_X86_64 || info->platform != STYLEOS_PLATFORM_UEFI ||
        info->flags != STYLEOS_BOOT_EXITED_SERVICES) return OP_PLATFORM;
    struct x86_state state = {.input = input};
    if (!read_state(&state) || !image_identity(&state, input->live.image) ||
        !descriptor_bounds_ok(&state, input->live.image)) return OP_PLATFORM;

    uint64_t gdt_va[2], gdt_pa[2];
    struct op_walk gdt_walks[2];
    size_t nr_gdt = 0;
    const uint64_t first = state.gdtr.base & ~(OP_PAGE - 1u);
    const uint64_t end = (state.gdtr.base + state.gdtr.limit + OP_PAGE) &
                         ~(OP_PAGE - 1u);
    for (uint64_t va = first; va < end; va += OP_PAGE) {
        struct old_mapping mapping;
        if (nr_gdt == 2u ||
            old_walk_record(&state, va, &mapping, &gdt_walks[nr_gdt]) != WALK_OK ||
            mapping.type != 6u || !reserved_page(input, mapping.pa))
            return OP_PLATFORM;
        gdt_walks[nr_gdt].kind = OP_WALK_GDT;
        gdt_va[nr_gdt] = va; gdt_pa[nr_gdt++] = mapping.pa;
    }
    if (!identity_tables(&state) || !descriptors_ok(&state, input->live.image))
        return OP_PLATFORM;
    for (size_t i = 0; i < nr_gdt; ++i)
        input->walks[input->nr_walks++] = gdt_walks[i];
    if (!record_identity_tables(&state, input)) return OP_PLATFORM;

    /* All image addresses are the actual loaded-PE identity mapping. GDT PA
     * came from the old walker; table pages are a known PARTIAL active set. */
    input->snapshot = snapshot_state(&state);
    struct op_snapshot after;
    if (!op_arch_unchanged(input, &after)) return OP_CONTEXT;
    if (input->nr_claims > OP_CLAIMS_MAX ||
        state.nr_visited + nr_gdt + 2u > OP_CLAIMS_MAX - input->nr_claims)
        return OP_LIMIT;
    enum op_error error = op_add_claim(input, OP_ROOT, OP_REGISTER,
                                       0, state.cr3, OP_PAGE);
    if (error) return error;
    for (size_t i = 0; i < state.nr_visited; ++i) {
        error = op_add_claim(input, OP_OLD_TABLE, OP_OLD_WALK,
                             state.visited[i], state.visited[i], OP_PAGE);
        if (error) return error;
    }
    for (size_t i = 0; i < nr_gdt; ++i) {
        error = op_add_claim(input, OP_GDT, OP_OLD_WALK, gdt_va[i], gdt_pa[i], OP_PAGE);
        if (error) return error;
    }
    error = op_add_claim(input, OP_IDT, OP_OLD_WALK,
                         state.idtr.base, state.idtr.base, 4096u);
    if (error) return error;
    input->unresolved |= OP_TABLE_SET_INCOMPLETE | OP_OTHER_FIRMWARE_STATE;
    return OP_OK;
}
