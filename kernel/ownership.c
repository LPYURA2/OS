#include "styleos/ownership.h"
#include "styleos/boot_pool.h"

/* This module consumes numeric evidence. It never dereferences a PA or returns
 * a page, and its only writes are to the caller's input builder/output/scratch. */
static int contains(struct boot_range outer, struct boot_range inner)
{
    return inner.start < inner.end && outer.start <= inner.start &&
           inner.end <= outer.end;
}

static int overlaps(struct boot_range a, struct boot_range b)
{
    return a.start < b.end && b.start < a.end;
}

static enum op_error extent(uint64_t base, uint64_t bytes, struct boot_range *r)
{
    if (!bytes) return OP_RANGE;
    if (base > UINT64_MAX - bytes) return OP_OVERFLOW;
    *r = (struct boot_range){base, base + bytes};
    return OP_OK;
}

static enum op_error round_out(struct boot_range r, struct boot_range *out)
{
    uint64_t extra = (OP_PAGE - (r.end & (OP_PAGE - 1u))) & (OP_PAGE - 1u);
    if (r.start >= r.end) return OP_RANGE;
    if (r.end > UINT64_MAX - extra) return OP_OVERFLOW;
    *out = (struct boot_range){r.start & ~(OP_PAGE - 1u), r.end + extra};
    return OP_OK;
}

static int known_kind(unsigned kind)
{
    return kind >= OP_IMAGE && kind <= OP_MMIO;
}

static int known_proof(unsigned proof)
{
    return proof <= OP_FIXED_DEVICE;
}

enum op_error op_add_claim(struct op_input *in, enum op_kind kind,
    enum op_proof proof, uint64_t va, uint64_t pa, uint64_t bytes)
{
    struct boot_range r;
    enum op_error e;
    if (!in) return OP_ARGUMENT;
    if (!known_kind(kind)) return OP_ARGUMENT;
    if (!known_proof(proof)) return OP_PROOF;
    if (in->nr_claims >= OP_CLAIMS_MAX) return OP_LIMIT;
    if ((e = extent(pa, bytes, &r)) || (e = extent(va, bytes, &r))) return e;
    in->claims[in->nr_claims++] = (struct op_claim){kind, proof, va, pa, bytes};
    return OP_OK;
}

static void unpublish(struct op_output *out)
{
    out->valid = 0;
    out->nr_protected = 0;
    out->nr_review = 0;
    out->protected_pages = 0;
    out->review_pages = 0;
    out->granted = 0;
}

static int within_pa(struct boot_range r, uint64_t limit)
{
    return r.start < r.end && r.end <= limit;
}

static int same(struct boot_range a, struct boot_range b)
{
    return a.start == b.start && a.end == b.end;
}

static int set_contains(const struct boot_range *ranges, size_t count,
                        struct boot_range r)
{
    for (size_t i = 0; i < count; ++i)
        if (contains(ranges[i], r)) return 1;
    return 0;
}

/* The union permits a dependency in firmware/non-candidate memory that is
 * already protected by the baseline. No unknown gap becomes known RAM. */
static int known_domain(const struct boot_memory_map *m, struct boot_range r)
{
    uint64_t cursor = r.start;
    while (cursor < r.end) {
        uint64_t end = cursor;
        for (size_t i = 0; i < m->nr_ram; ++i)
            if (m->ram[i].start <= cursor && m->ram[i].end > end)
                end = m->ram[i].end;
        for (size_t i = 0; i < m->nr_reserved; ++i)
            if (m->reserved[i].start <= cursor && m->reserved[i].end > end)
                end = m->reserved[i].end;
        if (end == cursor) return 0;
        cursor = end;
    }
    return 1;
}

static enum op_error check_map(const struct boot_memory_map *m,
                               struct op_scratch *scratch, uint64_t limit)
{
    if (!m || !m->nr_ram || !m->nr_candidates ||
        m->nr_ram > BOOT_MEMORY_INPUT_MAX ||
        m->nr_reserved > BOOT_MEMORY_INPUT_MAX ||
        m->nr_candidates > BOOT_MEMORY_RANGE_MAX) return OP_MAP;
    const struct boot_range *sets[] = {m->ram, m->reserved, m->candidates};
    const size_t counts[] = {m->nr_ram, m->nr_reserved, m->nr_candidates};
    for (size_t set = 0; set < 3; ++set) {
        for (size_t i = 0; i < counts[set]; ++i) {
            struct boot_range r = sets[set][i];
            if (!within_pa(r, limit)) return OP_MAP;
            if (set && ((r.start | r.end) & (OP_PAGE - 1u))) return OP_MAP;
            if (i && (r.start < sets[set][i - 1].end ||
                (set && r.start == sets[set][i - 1].end))) return OP_MAP;
        }
    }
    /* Reconstruct the candidate partition independently of the supplied list.
     * The old normalizer is used unchanged and never mutates input evidence. */
    scratch->checked = *m;
    if (bm_normalize(&scratch->checked) != BM_OK) return OP_MAP;
    const struct boot_memory_map *c = &scratch->checked;
    if (c->nr_ram != m->nr_ram || c->nr_reserved != m->nr_reserved ||
        c->nr_candidates != m->nr_candidates) return OP_MAP;
    for (size_t i = 0; i < m->nr_ram; ++i)
        if (!same(c->ram[i], m->ram[i])) return OP_MAP;
    for (size_t i = 0; i < m->nr_reserved; ++i)
        if (!same(c->reserved[i], m->reserved[i])) return OP_MAP;
    for (size_t i = 0; i < m->nr_candidates; ++i)
        if (!same(c->candidates[i], m->candidates[i])) return OP_MAP;
    return OP_OK;
}

static enum op_error check_context(const struct op_input *in, uint64_t limit)
{
    const struct boot_range image_objects[] = {
        in->live.stack, in->live.handoff, in->map_workspace,
        in->own_workspace, in->pool, in->pool_metadata
    };
    if (!within_pa(in->live.image, limit) || !within_pa(in->live.source, limit))
        return OP_RANGE;
    for (size_t i = 0; i < sizeof image_objects / sizeof image_objects[0]; ++i)
        if (!within_pa(image_objects[i], limit) ||
            !contains(in->live.image, image_objects[i])) return OP_CONTEXT;
    if (in->live.handoff.end - in->live.handoff.start != sizeof(struct boot_info) ||
        in->map_workspace.end - in->map_workspace.start != OP_DIAG_BYTES ||
        in->own_workspace.end - in->own_workspace.start != OP_WORKSPACE_BYTES ||
        in->pool.end - in->pool.start != BOOT_POOL_BYTES ||
        in->pool_metadata.end - in->pool_metadata.start != BOOT_POOL_METADATA_BYTES)
        return OP_CONTEXT;
    if ((in->own_workspace.start | in->own_workspace.end | in->pool.start |
         in->pool.end | in->pool_metadata.start | in->pool_metadata.end) &
        (OP_PAGE - 1u)) return OP_ALIGNMENT;
    const struct boot_range separate[] = {
        in->map_workspace, in->own_workspace, in->pool, in->pool_metadata
    };
    const struct boot_range live[] = {
        in->live.stack, in->live.handoff, in->live.source
    };
    for (size_t i = 0; i < sizeof separate / sizeof separate[0]; ++i) {
        for (size_t j = 0; j < i; ++j)
            if (overlaps(separate[i], separate[j])) return OP_CONTEXT;
        for (size_t j = 0; j < sizeof live / sizeof live[0]; ++j)
            if (overlaps(separate[i], live[j])) return OP_CONTEXT;
    }
    if (!set_contains(in->map->reserved, in->map->nr_reserved, in->live.image) ||
        !set_contains(in->map->reserved, in->map->nr_reserved, in->live.source))
        return OP_REQUIRED;
    return OP_OK;
}

static enum op_error check_platform(const struct op_input *in)
{
    const struct op_snapshot *s = &in->snapshot;
    uint64_t bytes = in->live.source.end - in->live.source.start;
    if (in->arch == STYLEOS_ARCH_X86_64) {
        if (in->source != OP_UEFI) return OP_SOURCE;
        if (s->phase != OP_INHERITED ||
            in->unresolved != (OP_TABLE_SET_INCOMPLETE | OP_OTHER_FIRMWARE_STATE))
            return OP_PLATFORM;
        if (in->source_version != 1 || in->source_stride < 40 ||
            (in->source_stride & 7u) || !bytes || bytes % in->source_stride)
            return OP_SOURCE;
        if ((s->mode & UINT64_C(0x80000001)) != UINT64_C(0x80000001) ||
            (s->mode & UINT64_C(0x60000000)) ||
            !(s->aux0 & (UINT64_C(1) << 5)) ||
            (s->aux0 & ((UINT64_C(1) << 12) | (UINT64_C(1) << 17) |
                       (UINT64_C(1) << 21) | (UINT64_C(1) << 22) |
                       (UINT64_C(1) << 23) | (UINT64_C(1) << 24))) ||
            (s->aux1 & UINT64_C(0xd00)) != UINT64_C(0xd00) ||
            (s->pat & 255u) != 6 || (s->rflags & (UINT64_C(1) << 9)) ||
            !(s->features & 1u) || (s->features & ~3u) ||
            !s->root || (s->root & (OP_PAGE - 1u)) ||
            !s->gdt_bytes || s->gdt_bytes > 65536 || (s->gdt_va & 7u) ||
            s->idt_bytes != OP_PAGE ||
            !s->cs || (s->cs & 7u) || s->cs >= s->gdt_bytes ||
            s->gdt_bytes - s->cs < 8) return OP_PLATFORM;
    } else if (in->arch == STYLEOS_ARCH_AARCH64) {
        if (in->source != OP_FDT) return OP_SOURCE;
        if (s->phase != OP_MMU_OFF || in->unresolved || s->root ||
            (s->mode & ((UINT64_C(1) << 25) | (UINT64_C(1) << 24) |
                        (UINT64_C(1) << 12) | 5u)) ||
            s->pat || s->rflags || s->features || in->nr_walks ||
            !s->aux0 || (s->aux0 & 2047u) ||
            (s->aux1 & UINT64_C(0x3c0)) != UINT64_C(0x3c0) ||
            s->gdt_va || s->gdt_bytes || s->idt_va || s->idt_bytes || s->cs)
            return OP_PLATFORM;
        if (in->source_stride || in->source_version || bytes < 40 ||
            bytes > BOOT_DTB_MAX) return OP_SOURCE;
    } else return OP_PLATFORM;
    return OP_OK;
}

static struct boot_range canonical(const struct op_input *in, unsigned kind)
{
    switch (kind) {
    case OP_IMAGE: return in->live.image;
    case OP_STACK: return in->live.stack;
    case OP_HANDOFF: return in->live.handoff;
    case OP_SOURCE_BUFFER: return in->live.source;
    case OP_MAP_WORKSPACE: return in->map_workspace;
    case OP_OWN_WORKSPACE: return in->own_workspace;
    case OP_BOOT_POOL: return in->pool;
    case OP_POOL_METADATA: return in->pool_metadata;
    default: return (struct boot_range){0, 0};
    }
}

static enum op_error check_claims(const struct op_input *in, uint64_t limit)
{
    unsigned found = 0, root = 0, idt = 0, vectors = 0, mmio = 0, tables = 0;
    struct boot_range gdt = {0};
    if (in->nr_claims > OP_CLAIMS_MAX) return OP_LIMIT;
    if (in->arch == STYLEOS_ARCH_X86_64 &&
        extent(in->snapshot.gdt_va, in->snapshot.gdt_bytes, &gdt)) return OP_OVERFLOW;
    for (size_t i = 0; i < in->nr_claims; ++i) {
        const struct op_claim *c = &in->claims[i];
        struct boot_range r, v, rounded;
        enum op_error e;
        if (!known_kind(c->kind)) return OP_ARGUMENT;
        if (!known_proof(c->proof)) return OP_PROOF;
        if ((e = extent(c->pa, c->bytes, &r)) ||
            (e = extent(c->va, c->bytes, &v)) || (e = round_out(r, &rounded))) return e;
        if (!within_pa(rounded, limit)) return OP_RANGE;
        if (c->kind <= OP_POOL_METADATA) {
            unsigned bit = 1u << c->kind;
            unsigned proof = c->kind == OP_SOURCE_BUFFER ? OP_BOOT_SOURCE : OP_IMAGE_IDENTITY;
            if (found & bit) return OP_CONTEXT;
            if (!same(r, canonical(in, c->kind)) || c->va != c->pa) return OP_CONTEXT;
            if (c->proof != proof) return OP_REQUIRED;
            found |= bit;
        } else if (in->arch == STYLEOS_ARCH_X86_64) {
            switch (c->kind) {
            case OP_GDT:
                if (c->proof != OP_OLD_WALK) return OP_REQUIRED;
                if (c->bytes != OP_PAGE || ((c->pa | c->va) & (OP_PAGE - 1u)) ||
                    !overlaps(v, gdt)) return OP_CONTEXT;
                break;
            case OP_IDT:
                if (idt++) return OP_CONTEXT;
                if (c->proof != OP_OLD_WALK) return OP_REQUIRED;
                if (c->bytes != OP_PAGE || c->va != in->snapshot.idt_va ||
                    c->pa != c->va || !contains(in->live.image, r)) return OP_CONTEXT;
                break;
            case OP_ROOT:
                if (root++) return OP_CONTEXT;
                if (c->proof != OP_REGISTER) return OP_REQUIRED;
                if (c->bytes != OP_PAGE || c->pa != in->snapshot.root || c->va)
                    return OP_CONTEXT;
                break;
            case OP_OLD_TABLE:
                for (size_t j = 0; j < i; ++j)
                    if (in->claims[j].kind == OP_OLD_TABLE && in->claims[j].pa == c->pa)
                        return OP_CONTEXT;
                if (++tables > OP_TABLES_MAX) return OP_LIMIT;
                if (c->proof != OP_OLD_WALK) return OP_REQUIRED;
                if (c->bytes != OP_PAGE || (c->pa & (OP_PAGE - 1u)) || c->va != c->pa)
                    return OP_CONTEXT;
                break;
            default: return OP_PLATFORM;
            }
        } else {
            switch (c->kind) {
            case OP_VECTORS:
                if (vectors++) return OP_CONTEXT;
                if (c->proof != OP_IMAGE_IDENTITY) return OP_REQUIRED;
                if (c->bytes != 2048 || c->va != in->snapshot.aux0 ||
                    c->pa != c->va || !contains(in->live.image, r)) return OP_CONTEXT;
                break;
            case OP_MMIO:
                if (mmio++) return OP_CONTEXT;
                if (c->proof != OP_FIXED_DEVICE) return OP_REQUIRED;
                if (c->va != UINT64_C(0x9000000) || c->pa != c->va || c->bytes != OP_PAGE)
                    return OP_CONTEXT;
                break;
            default: return OP_PLATFORM;
            }
        }
        if (!known_domain(in->map, rounded)) return OP_RANGE;
        if ((c->kind == OP_MMIO || c->kind == OP_GDT ||
             c->kind == OP_ROOT || c->kind == OP_OLD_TABLE) &&
            !set_contains(in->map->reserved, in->map->nr_reserved, rounded))
            return OP_REQUIRED;
        /* Same virtual bytes cannot prove two different physical translations.
         * ROOT's VA=0 records a register, not a translation. */
        if (c->kind != OP_ROOT) for (size_t j = 0; j < i; ++j) {
            const struct op_claim *other = &in->claims[j];
            struct boot_range old = {other->va, other->va + other->bytes};
            if (other->kind != OP_ROOT && overlaps(v, old)) {
                uint64_t start = v.start > old.start ? v.start : old.start;
                if (c->pa + (start - c->va) != other->pa + (start - other->va))
                    return OP_CONTEXT;
            }
        }
    }
    const unsigned mandatory = ((1u << (OP_POOL_METADATA + 1u)) - 1u) & ~1u;
    if (found != mandatory) return OP_REQUIRED;
    if (in->arch == STYLEOS_ARCH_AARCH64)
        return vectors && mmio ? OP_OK : OP_REQUIRED;
    if (!root || !idt || !tables) return OP_REQUIRED;
    /* Require every page covering GDTR's full byte extent. PA is taken from
     * each page's explicit walker claim; numerical VA=PA is never assumed. */
    struct boot_range pages;
    enum op_error e = round_out(gdt, &pages);
    if (e) return e;
    for (uint64_t page = pages.start; page < pages.end; page += OP_PAGE) {
        const struct op_claim *proof = 0;
        for (size_t i = 0; i < in->nr_claims; ++i)
            if (in->claims[i].kind == OP_GDT && in->claims[i].va == page) {
                if (proof) return OP_CONTEXT;
                proof = &in->claims[i];
            }
        if (!proof) return OP_REQUIRED;
    }
    return OP_OK;
}

static int table_claimed(const struct op_input *in, uint64_t pa)
{
    for (size_t i = 0; i < in->nr_claims; ++i)
        if (in->claims[i].kind == OP_OLD_TABLE && in->claims[i].pa == pa)
            return 1;
    return 0;
}

/* Raw descriptor evidence is an audit trail from the bounded architecture
 * walker. Recheck it without touching a table PA. This proves only the listed
 * paths; the deliberately incomplete old table set remains unresolved. */
static enum op_error check_walks(const struct op_input *in)
{
    if (in->arch != STYLEOS_ARCH_X86_64) return OP_OK;
    if (in->nr_walks > OP_WALKS_MAX) return OP_LIMIT;
    for (size_t i = 0; i < in->nr_walks; ++i) {
        const struct op_walk *w = &in->walks[i];
        if ((w->kind != OP_WALK_GDT && w->kind != OP_WALK_TABLE) ||
            w->levels < 2 || w->levels > 4) return OP_PROOF;
        for (size_t j = 0; j < i; ++j)
            if (in->walks[j].kind == w->kind && in->walks[j].va == w->va)
                return OP_PROOF;
    }
    const uint64_t address = UINT64_C(0x000ffffffffff000);
    const uint64_t pa_mask = ((UINT64_C(1) << in->snapshot.pa_bits) - 1u) & address;
    for (size_t i = 0; i < in->nr_claims; ++i) {
        const struct op_claim *c = &in->claims[i];
        if (c->kind != OP_GDT && c->kind != OP_OLD_TABLE) continue;
        unsigned kind = c->kind == OP_GDT ? OP_WALK_GDT : OP_WALK_TABLE;
        unsigned matches = 0;
        for (size_t j = 0; j < in->nr_walks; ++j)
            if (in->walks[j].kind == kind && in->walks[j].va == c->va &&
                in->walks[j].pa == c->pa) ++matches;
        if (!matches) return OP_REQUIRED;
        if (matches != 1) return OP_PROOF;
    }
    for (size_t i = 0; i < in->nr_walks; ++i) {
        const struct op_walk *w = &in->walks[i];
        unsigned claim_kind;
        if (w->kind == OP_WALK_GDT) claim_kind = OP_GDT;
        else if (w->kind == OP_WALK_TABLE) claim_kind = OP_OLD_TABLE;
        else return OP_PROOF;
        unsigned matches = 0;
        for (size_t j = 0; j < in->nr_claims; ++j)
            if (in->claims[j].kind == claim_kind && in->claims[j].va == w->va &&
                in->claims[j].pa == w->pa) ++matches;
        if (matches != 1) return OP_PROOF;
        if (w->levels < 2 || w->levels > 4 || w->tables[0] != in->snapshot.root ||
            (w->va & (OP_PAGE - 1u)) || (w->pa & (OP_PAGE - 1u))) return OP_PROOF;
        uint64_t upper = w->va >> 47;
        if (upper != 0 && upper != UINT64_C(0x1ffff)) return OP_PROOF;
        for (unsigned level = w->levels; level < 4; ++level)
            if (w->tables[level] || w->entries[level]) return OP_PROOF;
        for (unsigned level = 0; level < w->levels; ++level) {
            uint64_t table = w->tables[level], entry = w->entries[level];
            if (!table || (table & ~pa_mask) || !table_claimed(in, table) ||
                !(entry & 1u) || (entry & address & ~pa_mask) ||
                (entry & UINT64_C(0x7800000000000000))) return OP_PROOF;
            unsigned shift = 39u - level * 9u;
            unsigned index = (unsigned)((w->va >> shift) & 511u);
            /* A snapshot cannot contain two different translations/attributes for
             * one slot. Hardware may set Accessed/Dirty while evidence is
             * collected; these two bookkeeping bits do not change a mapping. */
            for (size_t j = 0; j <= i; ++j) {
                const struct op_walk *other = &in->walks[j];
                unsigned stop = j == i ? level : other->levels;
                if (stop > 4) return OP_PROOF;
                for (unsigned k = 0; k < stop; ++k)
                    if (other->tables[k] == table &&
                        ((other->va >> (39u - k * 9u)) & 511u) == index &&
                        ((other->entries[k] ^ entry) & ~UINT64_C(0x60))) return OP_PROOF;
            }
            int large = level < 3 && (entry & 128u);
            if (large && (!level || (level == 1 && !(in->snapshot.features & 2u))))
                return OP_PROOF;
            if (level == 3 || large) {
                if (level + 1u != w->levels) return OP_PROOF;
                unsigned leaf_shift = large ? shift : 12u;
                uint64_t offset = (UINT64_C(1) << leaf_shift) - 1u;
                if (large && (entry & address & offset & ~(UINT64_C(1) << 12)))
                    return OP_PROOF;
                uint64_t pa = (entry & pa_mask & ~offset) | (w->va & offset);
                unsigned pat_bit = (unsigned)((entry >> (large ? 12u : 7u)) & 1u);
                unsigned pat_index = (unsigned)((entry >> 3) & 3u) | (pat_bit << 2);
                if (pa != w->pa || ((in->snapshot.pat >> (pat_index * 8u)) & 255u) != 6u)
                    return OP_PROOF;
            } else {
                if (level + 1u == w->levels || (entry & 24u) ||
                    (entry & pa_mask) != w->tables[level + 1u]) return OP_PROOF;
            }
        }
    }
    return OP_OK;
}

static void sort(struct boot_range *ranges, size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        struct boot_range r = ranges[i];
        size_t j = i;
        while (j && r.start < ranges[j - 1].start) {
            ranges[j] = ranges[j - 1];
            --j;
        }
        ranges[j] = r;
    }
}

static enum op_error emit_review(struct op_output *out, uint64_t start, uint64_t end)
{
    if (start >= end) return OP_OK;
    if (out->nr_review && out->review[out->nr_review - 1].end == start) {
        out->review[out->nr_review - 1].end = end;
    } else {
        if (out->nr_review == OP_RANGES_MAX) return OP_LIMIT;
        out->review[out->nr_review++] = (struct boot_range){start, end};
    }
    out->review_pages += (end - start) / OP_PAGE;
    return OP_OK;
}

enum op_error op_build(const struct op_input *in, struct op_output *out,
                        struct op_scratch *scratch)
{
    enum op_error e;
    if (!out) return OP_ARGUMENT;
    unpublish(out);
    if (!in || !scratch) return OP_ARGUMENT;
    unsigned max_bits = in->arch == STYLEOS_ARCH_AARCH64 ? 48u : 52u;
    if (in->snapshot.pa_bits < 32 || in->snapshot.pa_bits > max_bits) return OP_PLATFORM;
    uint64_t limit = UINT64_C(1) << in->snapshot.pa_bits;
    if ((e = check_platform(in)) || (e = check_map(in->map, scratch, limit)) ||
        (e = check_context(in, limit)) || (e = check_claims(in, limit)) ||
        (e = check_walks(in))) return e;
    size_t count = in->map->nr_reserved;
    for (size_t i = 0; i < count; ++i) scratch->ranges[i] = in->map->reserved[i];
    for (size_t i = 0; i < in->nr_claims; ++i) {
        const struct op_claim *c = &in->claims[i];
        struct boot_range r = {c->pa, c->pa + c->bytes};
        e = round_out(r, &scratch->ranges[count++]);
        if (e) return e; /* Already checked, retained as a local invariant. */
    }
    sort(scratch->ranges, count);
    for (size_t i = 0; i < count; ++i) {
        struct boot_range r = scratch->ranges[i];
        if (out->nr_protected && r.start <= out->protected_ranges[out->nr_protected - 1].end) {
            struct boot_range *last = &out->protected_ranges[out->nr_protected - 1];
            if (r.end > last->end) last->end = r.end;
        } else {
            if (out->nr_protected == OP_RANGES_MAX) { e = OP_LIMIT; goto fail; }
            out->protected_ranges[out->nr_protected++] = r;
        }
    }
    for (size_t i = 0; i < out->nr_protected; ++i)
        out->protected_pages += (out->protected_ranges[i].end -
                                out->protected_ranges[i].start) / OP_PAGE;
    size_t cursor = 0;
    for (size_t i = 0; i < in->map->nr_candidates; ++i) {
        struct boot_range r = in->map->candidates[i];
        while (cursor < out->nr_protected && out->protected_ranges[cursor].end <= r.start)
            ++cursor;
        size_t cut = cursor;
        while (cut < out->nr_protected && out->protected_ranges[cut].start < r.end) {
            struct boot_range p = out->protected_ranges[cut];
            uint64_t end = p.start < r.end ? p.start : r.end;
            if ((e = emit_review(out, r.start, end))) goto fail;
            if (p.end > r.start) r.start = p.end;
            if (r.start >= r.end) break;
            ++cut;
        }
        if ((e = emit_review(out, r.start, r.end))) goto fail;
    }
    out->valid = 1;
    return OP_OK;
fail:
    unpublish(out);
    return e;
}

const char *op_error_name(enum op_error e)
{
    switch (e) {
    case OP_OK: return "ok";
    case OP_ARGUMENT: return "argument";
    case OP_RANGE: return "range";
    case OP_ALIGNMENT: return "alignment";
    case OP_OVERFLOW: return "overflow";
    case OP_LIMIT: return "limit";
    case OP_MAP: return "map";
    case OP_SOURCE: return "source";
    case OP_REQUIRED: return "required";
    case OP_PROOF: return "proof";
    case OP_PLATFORM: return "platform";
    case OP_CONTEXT: return "context";
    }
    return "unknown";
}

const char *op_kind_name(unsigned kind)
{
    switch (kind) {
    case OP_IMAGE: return "IMAGE";
    case OP_STACK: return "STACK";
    case OP_HANDOFF: return "HANDOFF";
    case OP_SOURCE_BUFFER: return "SOURCE_BUFFER";
    case OP_MAP_WORKSPACE: return "MAP_WORKSPACE";
    case OP_OWN_WORKSPACE: return "OWN_WORKSPACE";
    case OP_BOOT_POOL: return "BOOT_POOL";
    case OP_POOL_METADATA: return "POOL_METADATA";
    case OP_GDT: return "GDT";
    case OP_IDT: return "IDT";
    case OP_ROOT: return "ROOT";
    case OP_OLD_TABLE: return "OLD_TABLE";
    case OP_VECTORS: return "VECTORS";
    case OP_MMIO: return "MMIO";
    }
    return "unknown";
}

const char *op_proof_name(unsigned proof)
{
    switch (proof) {
    case OP_UNPROVEN: return "UNPROVEN";
    case OP_IMAGE_IDENTITY: return "IMAGE_IDENTITY";
    case OP_BOOT_SOURCE: return "BOOT_SOURCE";
    case OP_OLD_WALK: return "OLD_WALK";
    case OP_REGISTER: return "REGISTER";
    case OP_FIXED_DEVICE: return "FIXED_DEVICE";
    }
    return "unknown";
}
