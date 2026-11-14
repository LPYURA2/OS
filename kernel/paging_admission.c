#include "styleos/paging.h"
#include "styleos/boot_pool.h"

static int inside(struct boot_range a, struct boot_range b)
{
    return a.start < a.end && b.start <= a.start && a.end <= b.end;
}
static int overlap(struct boot_range a, struct boot_range b)
{
    return a.start < b.end && b.start < a.end;
}

enum pg_error pg_admit_layout(const struct boot_memory_evidence *e,
    const struct pg_storage *s, struct boot_range pool, struct boot_range metadata)
{
    if (!e || !s) return PG_ARGUMENT;
    /* Reuse the established read-only context/full-image reservation checks.
     * This does not admit or allocate from the boot pool. */
    if (pool.end < pool.start || metadata.end < metadata.start ||
        boot_pool_check_layout(pool.start, pool.end - pool.start,
            metadata.start, metadata.end - metadata.start, e) != BP_OK)
        return PG_CONTEXT;
    const struct boot_range own[] = {s->tables, s->control, s->data};
    const uint64_t size[] = {PG_TABLE_BYTES, PG_CONTROL_BYTES, PG_DATA_BYTES};
    const struct boot_range excluded[] = {e->live.stack, e->live.handoff,
        e->live.source, e->workspace, pool, metadata};
    for (size_t i = 0; i < 3; ++i) {
        if (own[i].start >= own[i].end || own[i].end - own[i].start != size[i] ||
            !inside(own[i], e->live.image)) return PG_RANGE;
        if ((own[i].start | own[i].end) & (PG_PAGE - 1u)) return PG_ALIGNMENT;
        for (size_t j = 0; j < sizeof excluded / sizeof excluded[0]; ++j)
            if (overlap(own[i], excluded[j])) return PG_CONFLICT;
        for (size_t j = 0; j < i; ++j)
            if (overlap(own[i], own[j])) return PG_CONFLICT;
    }
    return PG_OK;
}
