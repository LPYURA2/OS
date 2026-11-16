#include "styleos/tasks.h"

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

struct observation {
    const struct boot_memory_evidence *evidence;
    struct tasks_arch_state state;
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
    struct tasks_arch_state *s = &o->state;
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

static int descriptors(struct observation *o)
{
    struct tasks_arch_state *s = &o->state;
    const struct boot_range image = o->evidence->live.image;
    if (!s->cs || (s->cs & 7u) || o->gdtr.limit > 4095u ||
        s->cs > o->gdtr.limit || 7u > o->gdtr.limit - s->cs ||
        (s->gdt_base & 7u) || s->gdt_base >= LOW_LIMIT ||
        s->gdt_bytes > LOW_LIMIT - s->gdt_base ||
        s->idt_bytes != 4096u || !inside(image, s->idt_base, 4096u)) return 0;
    const uint64_t first = s->gdt_base & ~(PAGE - 1u);
    const uint64_t end = (s->gdt_base + s->gdt_bytes + PAGE - 1u) & ~(PAGE - 1u);
    for (uint64_t va = first; va < end; va += PAGE) {
        struct mapping m;
        if (s->gdt_pages == 2u || !walk(o, va, &m) || m.type != 6u ||
            !reserved_page(o, m.pa) || overlaps(image, m.pa) || overlaps(image, va))
            return 0;
        if (!s->gdt_pages) s->gdt_pa0 = m.pa;
        else s->gdt_pa1 = m.pa;
        ++s->gdt_pages;
    }
    /* Close the finite set actually used above.  New child pages discovered
     * by these identity walks join this same bounded loop. */
    for (size_t i = 0; i < o->nr_visited; ++i) {
        struct mapping m;
        if (!walk(o, o->visited[i], &m) || m.pa != o->visited[i] || m.type != 6u)
            return 0;
    }
    const uint64_t descriptor = load_le((const volatile unsigned char *)(uintptr_t)
                                       (s->gdt_base + s->cs), 8);
    const unsigned access = (unsigned)(descriptor >> 40) & 255u;
    if ((access & 0xf8u) != 0x98u || (access & 0x60u) ||
        !(descriptor & BIT(53)) || (descriptor & BIT(54))) return 0;
    const volatile unsigned char *gates =
        (const volatile unsigned char *)(uintptr_t)s->idt_base;
    for (unsigned i = 0; i < 256; ++i) {
        const uint64_t low = load_le(gates + 16u * i, 8);
        const uint64_t high = load_le(gates + 16u * i + 8u, 8);
        const uint64_t target = (low & 65535u) | ((low >> 48) << 16) | (high << 32);
        struct mapping m;
        if (((low >> 16) & 65535u) != s->cs || ((low >> 32) & 255u) ||
            ((low >> 40) & 255u) != 0x8eu || high >> 32 ||
            !inside(image, target, 1u) || !walk(o, target, &m) ||
            m.pa != target || m.type != 6u || !m.executable) return 0;
    }
    return 1;
}

enum tasks_error tasks_arch_observe(const struct boot_memory_evidence *e,
                                   struct tasks_arch_state *out)
{
    if (!e || !out || !e->map) return TASKS_ARGUMENT;
    if (!inside(e->live.image, e->live.handoff.start, sizeof(struct boot_info)) ||
        e->live.handoff.end - e->live.handoff.start != sizeof(struct boot_info))
        return TASKS_PLATFORM;
    const struct boot_info *info = (const struct boot_info *)(uintptr_t)e->live.handoff.start;
    if (info->arch != STYLEOS_ARCH_X86_64 || info->platform != STYLEOS_PLATFORM_UEFI ||
        info->flags != STYLEOS_BOOT_EXITED_SERVICES) return TASKS_PLATFORM;
    uint64_t sp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(sp));
    if ((sp & 15u) || !inside(e->live.stack, sp, 16u)) return TASKS_STACK;
    struct observation o = {.evidence = e};
    if (!read_state(&o) || !image_identity(&o) || !pe_header(&o) ||
        !writable_image(&o, (uint64_t)(uintptr_t)styleos_tasks_stack_a, TASKS_STACK_BYTES) ||
        !writable_image(&o, (uint64_t)(uintptr_t)styleos_tasks_stack_b, TASKS_STACK_BYTES) ||
        !writable_image(&o, (uint64_t)(uintptr_t)&styleos_tasks_control, TASKS_CONTROL_BYTES) ||
        !descriptors(&o)) return TASKS_PLATFORM;
    *out = o.state;
    return TASKS_OK;
}

int tasks_arch_unchanged(const struct tasks_arch_state *a,
                         const struct tasks_arch_state *b)
{
    /* Arithmetic RFLAGS are caller-clobbered.  IF and DF must stay zero. */
    return a && b && a->mode == b->mode && a->root == b->root &&
        a->control0 == b->control0 && a->control1 == b->control1 &&
        !(a->flags & (BIT(9) | BIT(10))) && !(b->flags & (BIT(9) | BIT(10))) &&
        a->cs == b->cs && a->idt_base == b->idt_base && a->idt_bytes == b->idt_bytes &&
        a->gdt_base == b->gdt_base && a->gdt_bytes == b->gdt_bytes &&
        a->pat == b->pat && a->pa_bits == b->pa_bits && a->gdt_pages == b->gdt_pages &&
        a->gdt_pa0 == b->gdt_pa0 && a->gdt_pa1 == b->gdt_pa1;
}
