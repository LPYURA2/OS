#include "styleos/boot_pool.h"

/* Both complete objects belong to the loaded image. The metadata has a fixed,
 * separately checkable extent; none of its bytes is ever issued as a page. */
_Alignas(BOOT_POOL_PAGE) unsigned char
    styleos_bootpool_storage[BOOT_POOL_BYTES];
union boot_pool_control {
    struct { uint32_t admitted, used; } state;
    unsigned char padding[BOOT_POOL_METADATA_BYTES];
};
_Alignas(BOOT_POOL_PAGE) union boot_pool_control styleos_bootpool_metadata;
_Static_assert(sizeof styleos_bootpool_storage == 32768, "pool extent");
_Static_assert(sizeof styleos_bootpool_metadata == 4096, "metadata extent");
_Static_assert(BOOT_POOL_PAGES <= 32, "allocation bitmap width");

static int valid(struct boot_range r)
{
    return r.start < r.end;
}

static int contains(struct boot_range outer, struct boot_range inner)
{
    return outer.start <= inner.start && inner.end <= outer.end;
}

static int overlaps(struct boot_range a, struct boot_range b)
{
    return a.start < b.end && b.start < a.end;
}

static int extent(uint64_t base, uint64_t bytes, struct boot_range *out)
{
    if (bytes > UINT64_MAX - base) return 0;
    *out = (struct boot_range){base, base + bytes};
    return 1;
}

struct boot_range boot_pool_storage_range(void)
{
    uint64_t base = (uint64_t)(uintptr_t)styleos_bootpool_storage;
    return (struct boot_range){base, base + sizeof styleos_bootpool_storage};
}

struct boot_range boot_pool_metadata_range(void)
{
    uint64_t base = (uint64_t)(uintptr_t)&styleos_bootpool_metadata;
    return (struct boot_range){base, base + sizeof styleos_bootpool_metadata};
}

enum boot_pool_error boot_pool_check_layout(uint64_t base, uint64_t bytes,
    uint64_t metadata_base, uint64_t metadata_bytes,
    const struct boot_memory_evidence *e)
{
    struct boot_range pool, metadata, proof, map;
    if (!e) return BP_BAD_ARGUMENT;
    if ((uintptr_t)e % _Alignof(struct boot_memory_evidence)) return BP_CONTEXT;
    if (!extent((uint64_t)(uintptr_t)e, sizeof *e, &proof)) return BP_OVERFLOW;
    if (bytes != BOOT_POOL_BYTES || metadata_bytes != BOOT_POOL_METADATA_BYTES)
        return BP_RANGE;
    if (!extent(base, bytes, &pool) ||
        !extent(metadata_base, metadata_bytes, &metadata)) return BP_OVERFLOW;
    if ((pool.start | pool.end | metadata.start | metadata.end) &
        (BOOT_POOL_PAGE - 1u)) return BP_ALIGNMENT;

    /* This internal evidence is supplied by the successful diagnostic. Check
     * its complete mapped extents before reading map fields. These casts only
     * compare known kernel objects in the platform's image address space;
     * they do not translate arbitrary pointers into physical addresses. */
    if (!valid(e->live.image) || !valid(e->live.stack) ||
        !valid(e->live.handoff) || !valid(e->live.source) ||
        !valid(e->workspace) || !e->map) return BP_CONTEXT;
    if (!extent((uint64_t)(uintptr_t)e->map, sizeof *e->map, &map))
        return BP_OVERFLOW;
    if ((uintptr_t)e->map % _Alignof(struct boot_memory_map)) return BP_CONTEXT;
    if (!contains(e->live.image, e->live.stack) ||
        !contains(e->live.image, e->live.handoff) ||
        !contains(e->live.image, e->workspace) ||
        !contains(e->workspace, proof) || !contains(e->workspace, map) ||
        overlaps(proof, map)) return BP_CONTEXT;
    if (!contains(e->live.image, pool) || !contains(e->live.image, metadata))
        return BP_RANGE;

    const struct boot_range excluded[] = {
        e->live.stack, e->live.handoff, e->live.source, e->workspace
    };
    if (overlaps(pool, metadata)) return BP_OVERLAP;
    for (size_t i = 0; i < sizeof excluded / sizeof excluded[0]; ++i)
        if (overlaps(pool, excluded[i]) || overlaps(metadata, excluded[i]))
            return BP_OVERLAP;

    if (e->map->nr_reserved > BOOT_MEMORY_INPUT_MAX) return BP_CONTEXT;
    int image_reserved = 0;
    for (size_t i = 0; i < e->map->nr_reserved; ++i) {
        struct boot_range r = e->map->reserved[i];
        if (!valid(r)) return BP_CONTEXT;
        if (contains(r, e->live.image)) image_reserved = 1;
    }
    /* Normalization merged the image reservation into one complete extent.
     * Candidate entries are intentionally never inspected or granted. */
    return image_reserved ? BP_OK : BP_NOT_RESERVED;
}

enum boot_pool_error boot_pool_admit(const struct boot_memory_evidence *e)
{
    if (styleos_bootpool_metadata.state.admitted) return BP_ALREADY_READY;
    struct boot_range pool = boot_pool_storage_range();
    struct boot_range metadata = boot_pool_metadata_range();
    enum boot_pool_error error = boot_pool_check_layout(pool.start,
        sizeof styleos_bootpool_storage, metadata.start,
        sizeof styleos_bootpool_metadata, e);
    if (error != BP_OK) return error;
    styleos_bootpool_metadata.state.used = 0;
    styleos_bootpool_metadata.state.admitted = 1;
    return BP_OK;
}

struct boot_pool_result boot_pool_alloc(void)
{
    if (!styleos_bootpool_metadata.state.admitted)
        return (struct boot_pool_result){0, BP_NOT_READY};
    for (unsigned i = 0; i < BOOT_POOL_PAGES; ++i) {
        uint32_t bit = UINT32_C(1) << i;
        if (styleos_bootpool_metadata.state.used & bit) continue;
        unsigned char *page = styleos_bootpool_storage + i * BOOT_POOL_PAGE;
        for (size_t n = 0; n < BOOT_POOL_PAGE; ++n) page[n] = 0;
        styleos_bootpool_metadata.state.used |= bit;
        return (struct boot_pool_result){page, BP_OK};
    }
    return (struct boot_pool_result){0, BP_EXHAUSTED};
}

enum boot_pool_error boot_pool_free(void *page)
{
    if (!styleos_bootpool_metadata.state.admitted) return BP_NOT_READY;
    struct boot_range pool = boot_pool_storage_range();
    uint64_t address = (uint64_t)(uintptr_t)page;
    if (address < pool.start || address >= pool.end) return BP_OUTSIDE;
    if (address & (BOOT_POOL_PAGE - 1u)) return BP_ALIGNMENT;
    unsigned index = (unsigned)((address - pool.start) / BOOT_POOL_PAGE);
    uint32_t bit = UINT32_C(1) << index;
    if (!(styleos_bootpool_metadata.state.used & bit)) return BP_DOUBLE_FREE;
    styleos_bootpool_metadata.state.used &= ~bit;
    return BP_OK;
}

const char *boot_pool_error_name(enum boot_pool_error error)
{
    switch (error) {
    case BP_OK: return "ok";
    case BP_NOT_READY: return "not-ready";
    case BP_ALREADY_READY: return "already-ready";
    case BP_BAD_ARGUMENT: return "bad-argument";
    case BP_RANGE: return "range";
    case BP_OVERFLOW: return "overflow";
    case BP_ALIGNMENT: return "alignment";
    case BP_OVERLAP: return "overlap";
    case BP_NOT_RESERVED: return "not-reserved";
    case BP_EXHAUSTED: return "exhausted";
    case BP_OUTSIDE: return "outside";
    case BP_DOUBLE_FREE: return "double-free";
    case BP_CONTEXT: return "context";
    }
    return "unknown";
}
