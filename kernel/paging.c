#include "styleos/paging.h"

/* These constructors only write the admitted, caller-owned table array. They
 * never allocate RAM, publish a hardware register, or infer PA from a pointer.
 * The caller establishes the relationship between tables and tables_pa. */
#define PG_VA_LIMIT (UINT64_C(1) << 47)
#define PG_NX (UINT64_C(1) << 63)
#define PG_ARM_PXN (UINT64_C(1) << 53)
#define PG_ARM_UXN (UINT64_C(1) << 54)

static uint64_t address_mask(const struct pg_plan *p)
{
    return ((UINT64_C(1) << p->pa_bits) - 1u) & ~UINT64_C(4095);
}

static int overlap(uint64_t a, uint64_t bytes, uint64_t b, uint64_t other)
{
    /* Every addition here has already passed plan validation. */
    return a < b + other && b < a + bytes;
}

static enum pg_error validate(const struct pg_plan *p)
{
    if (!p || (uintptr_t)p % _Alignof(struct pg_plan)) return PG_ARGUMENT;
    if (sizeof *p > UINTPTR_MAX - (uintptr_t)p) return PG_OVERFLOW;
    if (p->arch != PG_X86 && p->arch != PG_ARM) return PG_ARGUMENT;
    if (p->pa_bits < 32 || p->pa_bits > (p->arch == PG_X86 ? 52u : 48u))
        return PG_WIDTH;
    if (!p->tables || !p->tables_pa) return PG_ARGUMENT;
    if (!p->capacity || p->capacity > PG_TABLE_PAGES) return PG_CAPACITY;
    if (((uintptr_t)p->tables | p->tables_pa) & (PG_PAGE - 1u))
        return PG_ALIGNMENT;
    const uint64_t table_bytes = p->capacity * (uint64_t)PG_PAGE;
    if (table_bytes > UINT64_MAX - p->tables_pa ||
        table_bytes > UINTPTR_MAX - (uintptr_t)p->tables) return PG_OVERFLOW;
    if ((uintptr_t)p < (uintptr_t)p->tables + table_bytes &&
        (uintptr_t)p->tables < (uintptr_t)p + sizeof *p) return PG_CONTEXT;
    const uint64_t pa_limit = UINT64_C(1) << p->pa_bits;
    if (p->tables_pa + table_bytes > pa_limit) return PG_WIDTH;
    if (!p->count || p->count > PG_MAX_MAPS) return PG_CAPACITY;
    if (p->probe & (PG_PAGE - 1u)) return PG_ALIGNMENT;
    if (p->probe >= PG_VA_LIMIT) return PG_RANGE;

    for (size_t i = 0; i < p->count; ++i) {
        const struct pg_mapping *m = &p->maps[i];
        if (!m->bytes) return PG_RANGE;
        if ((m->va | m->pa | m->bytes) & (PG_PAGE - 1u))
            return PG_ALIGNMENT;
        if (m->bytes > UINT64_MAX - m->va ||
            m->bytes > UINT64_MAX - m->pa) return PG_OVERFLOW;
        if (m->va >= PG_VA_LIMIT || m->va + m->bytes > PG_VA_LIMIT)
            return PG_RANGE;
        if (m->pa + m->bytes > pa_limit) return PG_WIDTH;
        if (m->flags & ~(PG_WRITE | PG_EXEC | PG_DEVICE)) return PG_RIGHTS;
        /* No x86 device-memory profile is claimed without validating PAT.
         * COM1 needs no memory mapping. ARM has an explicit MAIR slot. */
        if ((m->flags & PG_DEVICE) &&
            (p->arch == PG_X86 || (m->flags & PG_EXEC))) return PG_RIGHTS;
        if (m->va <= p->probe && p->probe < m->va + m->bytes)
            return PG_PROBE_MAPPED;
        for (size_t n = 0; n < i; ++n) {
            const struct pg_mapping *owner = &p->maps[n];
            if (overlap(m->va, m->bytes, owner->va, owner->bytes))
                return PG_CONFLICT;
            if (overlap(m->pa, m->bytes, owner->pa, owner->bytes) &&
                (((m->flags ^ owner->flags) & (PG_WRITE | PG_DEVICE)) ||
                 ((m->flags & PG_EXEC) && !(owner->flags & PG_EXEC))))
                return PG_RIGHTS;
        }
    }
    return PG_OK;
}

static uint64_t leaf_bits(enum pg_arch arch, unsigned flags)
{
    if (arch == PG_X86)
        return UINT64_C(1) | ((flags & PG_WRITE) ? UINT64_C(2) : 0) |
               ((flags & PG_EXEC) ? 0 : PG_NX);
    /* EL1 only, AF pre-set. Normal-NC is MAIR[0]=0x44, inner-shareable;
     * Device-nGnRnE is MAIR[1]=0, outer-shareable. User execution is denied. */
    return UINT64_C(3) | ((flags & PG_DEVICE) ? UINT64_C(4) : 0) |
           ((flags & PG_WRITE) ? 0 : UINT64_C(128)) |
           ((flags & PG_DEVICE) ? UINT64_C(512) : UINT64_C(768)) |
           UINT64_C(1024) | PG_ARM_UXN |
           ((flags & PG_EXEC) ? 0 : PG_ARM_PXN);
}

static struct pg_result refused(enum pg_error error)
{
    /* A failed attempt, including exhausted scratch tables, can never be
     * mistaken for a root that is ready for the architecture to activate. */
    return (struct pg_result){0, 0, error};
}

struct pg_result pg_build(const struct pg_plan *p)
{
    enum pg_error error = validate(p);
    if (error != PG_OK) return refused(error);
    for (size_t i = 0; i < p->capacity; ++i)
        for (size_t n = 0; n < 512; ++n) p->tables[i][n] = 0;
    size_t used = 1;
    const uint64_t mask = address_mask(p);
    for (size_t i = 0; i < p->count; ++i) {
        const struct pg_mapping *m = &p->maps[i];
        for (uint64_t offset = 0; offset < m->bytes; offset += PG_PAGE) {
            const uint64_t va = m->va + offset;
            size_t table = 0;
            for (unsigned level = 0; level < 3; ++level) {
                const unsigned shift = 39u - level * 9u;
                const size_t index = (size_t)((va >> shift) & 511u);
                uint64_t entry = p->tables[table][index];
                if (!entry) {
                    if (used == p->capacity) return refused(PG_CAPACITY);
                    const uint64_t child = p->tables_pa + used * PG_PAGE;
                    p->tables[table][index] = child | UINT64_C(3);
                    table = used++;
                } else {
                    /* Only builder-owned descriptors can exist at this point. */
                    const uint64_t child = entry & mask;
                    if ((entry & ~mask) != UINT64_C(3) || child < p->tables_pa ||
                        child - p->tables_pa >= used * PG_PAGE)
                        return refused(PG_CONTEXT);
                    table = (size_t)((child - p->tables_pa) / PG_PAGE);
                }
            }
            const size_t index = (size_t)((va >> 12) & 511u);
            if (p->tables[table][index]) return refused(PG_CONFLICT);
            p->tables[table][index] = (m->pa + offset) |
                                     leaf_bits(p->arch, m->flags);
        }
    }
    return (struct pg_result){p->tables_pa, used, PG_OK};
}

enum pg_error pg_lookup(const struct pg_plan *p, uint64_t va,
                         struct pg_lookup *out)
{
    if (!out) return PG_ARGUMENT;
    enum pg_error error = validate(p);
    if (error != PG_OK) return error;
    if (va >= PG_VA_LIMIT) return PG_RANGE;
    const uint64_t mask = address_mask(p);
    size_t table = 0, visited[4] = {0};
    for (unsigned level = 0; level < 4; ++level) {
        const unsigned shift = 39u - level * 9u;
        uint64_t entry = p->tables[table][(va >> shift) & 511u];
        if (!(entry & 1u)) return PG_RANGE;
        if (level != 3) {
            /* x86 hardware may set Accessed on a non-leaf entry. No user,
             * huge-page, NX-table or other permissions are constructed. */
            const uint64_t mutable_bits = p->arch == PG_X86 ? UINT64_C(32) : 0;
            if ((entry & ~(mask | mutable_bits)) != UINT64_C(3))
                return PG_RIGHTS;
            const uint64_t child = entry & mask;
            if (child < p->tables_pa ||
                child - p->tables_pa >= p->capacity * (uint64_t)PG_PAGE)
                return PG_CONTEXT;
            table = (size_t)((child - p->tables_pa) / PG_PAGE);
            for (unsigned n = 0; n <= level; ++n)
                if (visited[n] == table) return PG_CONTEXT;
            visited[level + 1] = table;
            continue;
        }
        unsigned flags;
        if (p->arch == PG_X86) {
            flags = ((entry & 2u) ? PG_WRITE : 0) |
                    ((entry & PG_NX) ? 0 : PG_EXEC);
            /* Accessed and Dirty are the only mutable leaf bits. */
            if ((entry & ~(mask | UINT64_C(96))) != leaf_bits(p->arch, flags))
                return PG_RIGHTS;
        } else {
            flags = ((entry & UINT64_C(192)) == 0 ? PG_WRITE : 0) |
                    ((entry & PG_ARM_PXN) ? 0 : PG_EXEC) |
                    ((entry & UINT64_C(28)) == 4 ? PG_DEVICE : 0);
            if ((flags & PG_DEVICE) && (flags & PG_EXEC)) return PG_RIGHTS;
            if ((entry & ~mask) != leaf_bits(p->arch, flags)) return PG_RIGHTS;
        }
        *out = (struct pg_lookup){(entry & mask) | (va & (PG_PAGE - 1u)), flags};
        return PG_OK;
    }
    return PG_CONTEXT;
}

const char *pg_error_name(enum pg_error error)
{
    switch (error) {
    case PG_OK: return "ok";
    case PG_ARGUMENT: return "argument";
    case PG_RANGE: return "range";
    case PG_ALIGNMENT: return "alignment";
    case PG_OVERFLOW: return "overflow";
    case PG_CONFLICT: return "conflict";
    case PG_CAPACITY: return "capacity";
    case PG_WIDTH: return "width";
    case PG_RIGHTS: return "rights";
    case PG_PROBE_MAPPED: return "probe-mapped";
    case PG_CONTEXT: return "context";
    case PG_PLATFORM: return "platform";
    }
    return "unknown";
}
