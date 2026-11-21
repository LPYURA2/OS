#include <styleos/frame_grant_proof.h>
#include <styleos/boot_pool.h>

#define FG_PAGE UINT64_C(4096)
#define FG_PHYS_LIMIT (UINT64_C(1) << 52)

static int range_ok(struct boot_range r)
{ return r.start < r.end; }
static int overlap(struct boot_range a, struct boot_range b)
{ return a.start < b.end && b.start < a.end; }
static int covered(struct boot_range a, struct boot_range b)
{ return a.start <= b.start && b.end <= a.end; }
static int extent(uint64_t start, uint64_t bytes, struct boot_range *out)
{
    if (!start || !bytes || (start & (FG_PAGE-1u)) ||
        (bytes & (FG_PAGE-1u)) || start >= FG_PHYS_LIMIT ||
        bytes > FG_PHYS_LIMIT - start) return 0;
    *out = (struct boot_range){start,start+bytes}; return 1;
}
static enum fg_status policy(const struct boot_memory_evidence *e,
                              struct boot_range r)
{
    if (!covered(e->live.image, r)) return FG_NOT_IMAGE;
    const struct boot_memory_map *m = e->map;
    int reserved = 0;
    for (size_t i=0; i<m->nr_reserved; ++i)
        if (covered(m->reserved[i], r)) reserved=1;
    if (!reserved) return FG_NOT_RESERVED;
    for (size_t i=0; i<m->nr_candidates; ++i)
        if (overlap(m->candidates[i], r)) return FG_CANDIDATE;
    const struct boot_range excluded[] = {
        e->live.stack, e->live.handoff, e->live.source, e->workspace,
        boot_pool_storage_range(), boot_pool_metadata_range()
    };
    for (size_t i=0; i<sizeof(excluded)/sizeof(excluded[0]); ++i)
        if (range_ok(excluded[i]) && overlap(excluded[i],r)) return FG_CONFLICT;
    return FG_OK;
}
enum fg_status fg_check_policy(const struct boot_memory_evidence *e,
                                const struct fg_claims *claims)
{
    if (!e || !e->map || !claims || !range_ok(e->live.image) ||
        e->map->nr_reserved > BOOT_MEMORY_INPUT_MAX ||
        e->map->nr_candidates > BOOT_MEMORY_RANGE_MAX) return FG_ARGUMENT;
    const uint64_t span = sizeof(struct dvm_storage);
    if (span != (uint64_t)DVM_ROOTS*DVM_TABLES_PER_ROOT*FG_PAGE)
        return FG_LAYOUT;
    struct boot_range table;
    if (!extent(claims->tables,span,&table)) return FG_LAYOUT;
    enum fg_status rc=policy(e,table);
    if (rc != FG_OK) return rc;
    struct boot_range frames[DVM_USER_SLOTS*DVM_USER_PAGES];
    size_t count=0;
    for (unsigned i=0;i<DVM_USER_SLOTS;++i)
        for (unsigned j=0;j<DVM_USER_PAGES;++j) {
            struct boot_range r;
            if (!extent(claims->user[i][j],FG_PAGE,&r)) return FG_LAYOUT;
            if (overlap(r,table)) return FG_CONFLICT;
            for(size_t k=0;k<count;++k)
                if(overlap(r,frames[k])) return FG_CONFLICT;
            rc=policy(e,r);
            if(rc!=FG_OK) return rc;
            frames[count++]=r;
        }
    return FG_OK;
}
