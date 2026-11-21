#include <styleos/desktop_vm_owner.h>

static void erase(void *p, size_t n)
{
    if (!p) return;
    volatile unsigned char *bytes = (volatile unsigned char *)p;
    while (n--) *bytes++ = 0;
}

static bool extent_ok(struct boot_range r)
{ return r.start < r.end; }

static bool inside(struct boot_range inner, struct boot_range outer)
{
    return extent_ok(inner) && extent_ok(outer) &&
           inner.start >= outer.start && inner.end <= outer.end;
}

static bool overlap(struct boot_range a, struct boot_range b)
{
    return extent_ok(a) && extent_ok(b) &&
           a.start < b.end && b.start < a.end;
}

static bool reserved_exactly(const struct boot_memory_map *map,
                             struct boot_range r)
{
    if (!map || map->nr_reserved > BOOT_MEMORY_INPUT_MAX) return false;
    for (size_t i = 0; i < map->nr_reserved; ++i)
        if (inside(r, map->reserved[i])) return true;
    return false;
}

static enum dvo_status reject(enum dvo_status why, struct dvm_storage *storage,
                              struct dvm_output *out)
{
    /* Never publish stale roots. Before the physical witness succeeds,
     * touching the alleged arena would itself be unsafe, even to zero it.
     * The page-table builder clears its own storage on any later failure. */
    if (why == DVO_BUILD) erase(storage, storage ? sizeof(*storage) : 0);
    erase(out, out ? sizeof(*out) : 0);
    return why;
}

enum dvo_status dvo_build_owned(const struct boot_memory_evidence *e,
                                struct dvo_arena arena,
                                struct dvo_exclusions x,
                                const struct dvm_plan *p,
                                struct dvm_storage *storage,
                                struct dvm_output *out,
                                dvo_identity_witness witness,
                                void *context)
{
    if (!storage || !out || storage == (void *)out ||
        !e || !p || !e->map || !witness) return
            reject(DVO_ARGUMENT, storage, out);
    if (arena.bytes != DVO_OWNED_BYTES ||
        !arena.base || (arena.base & (DVM_PAGE_SIZE - 1u)) ||
        arena.base > UINT64_MAX - arena.bytes ||
        (uint64_t)(uintptr_t)storage != arena.base)
        return reject(DVO_ALIGNMENT, storage, out);
    const struct boot_range region = {arena.base, arena.base + arena.bytes};
    if (!inside(region, e->live.image) ||
        e->live.image.start >= e->live.image.end)
        return reject(DVO_SCOPE, storage, out);
    if (!reserved_exactly(e->map, e->live.image) ||
        !reserved_exactly(e->map, region))
        return reject(DVO_RESERVED, storage, out);
    const struct boot_range forbidden[] = {
        e->live.stack, e->live.handoff, e->live.source,
        e->workspace, x.pool, x.pool_metadata
    };
    for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); ++i)
        if (overlap(region, forbidden[i]))
            return reject(DVO_OVERLAP, storage, out);
    if (p->physical_table_base != arena.base ||
        p->kernel_count == 0 || p->kernel_count > DVM_KERNEL_MAX_SPANS)
        return reject(DVO_LAYOUT, storage, out);
    for (size_t k = 0; k < p->kernel_count; ++k) {
        const struct dvm_region *r = &p->kernel[k];
        if (!r->pages ||
            r->pa > UINT64_MAX - (uint64_t)r->pages * DVM_PAGE_SIZE ||
            overlap(region, (struct boot_range){r->pa,
                       r->pa + (uint64_t)r->pages * DVM_PAGE_SIZE}))
            return reject(DVO_OVERLAP, storage, out);
    }
    /* Exact fixed physical provenance: no caller-selected or recycled PAs. */
    for (unsigned slot = 0; slot < DVM_USER_SLOTS; ++slot)
        for (unsigned j = 0; j < DVM_USER_PAGES; ++j) {
            const uint64_t i = DVO_TABLE_PAGES +
                               slot * DVM_USER_PAGES + j;
            if (p->user[slot][j].pa != arena.base + i * DVM_PAGE_SIZE ||
                p->user[slot][j].pages != 1u)
                return reject(DVO_LAYOUT, storage, out);
        }
    /* Prove actual inherited translations BEFORE the first table mutation.
     * Runtime has no default witness, hence no path to bypass this check. */
    for (unsigned i = 0; i < DVO_OWNED_PAGES; ++i) {
        const uint64_t page = arena.base + (uint64_t)i * DVM_PAGE_SIZE;
        if (!witness(context, page, page))
            return reject(DVO_PROOF, storage, out);
    }
    if (dvm_build(p, storage, out) != DVM_OK)
        return reject(DVO_BUILD, storage, out);
    /* Verify the produced root references the exact admitted arena. */
    for (unsigned root = 0; root < DVM_ROOTS; ++root)
        if (out->root[root] != arena.base +
                                  (uint64_t)root * DVM_TABLES_PER_ROOT * DVM_PAGE_SIZE ||
            !out->used[root] || out->used[root] > DVM_TABLES_PER_ROOT)
            return reject(DVO_BUILD, storage, out);
    return DVO_OK;
}
