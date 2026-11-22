#include <styleos/tss_ist_stage_782.h>
#include <styleos/boot_pool.h>
#define PAGE TIS782_PAGE
#define PHYS_LIMIT (UINT64_C(1)<<47)
static bool overlap(struct boot_range a,struct boot_range b)
{ return a.start<b.end && b.start<a.end; }
static bool contains(struct boot_range outer, struct boot_range r)
{ return outer.start<=r.start && r.end<=outer.end; }
static bool valid_range(struct boot_range r)
{ return r.start<r.end; }

enum tis782_status tis782_prepare(const struct boot_memory_evidence *e,
    const struct fg_claims *prior, const struct tis782_claims *claims,
    tis782_attest attest, void *context, struct tis782_plan *out)
{
    if(out) *out=(struct tis782_plan){0};
    if(!e || !e->map || !prior || !claims || !out || !attest ||
       e->map->nr_reserved>BOOT_MEMORY_INPUT_MAX ||
       e->map->nr_candidates>BOOT_MEMORY_RANGE_MAX ||
       fg_check_policy(e,prior)!=FG_OK) return TIS782_ARGUMENT;
    const uint64_t p[]={claims->gdt,claims->tss,claims->rsp0,claims->ist1};
    const struct boot_memory_map *m=e->map;
    const struct boot_range protected[]={e->live.stack,e->live.handoff,
        e->live.source,e->workspace,
        boot_pool_storage_range(),boot_pool_metadata_range()};
    const uint64_t baseline_tables=prior->tables;
    const uint64_t baseline_span=sizeof(struct dvm_storage);
    if(baseline_span/PAGE!=72u || baseline_span%PAGE) return TIS782_ARGUMENT;
    const struct boot_range t={baseline_tables,baseline_tables+baseline_span};
    if(t.end<baseline_tables) return TIS782_ARGUMENT;
    for(unsigned i=0;i<4u;++i) {
        uint64_t start=p[i];
        if(!start || (start&(PAGE-1u)) || start>=PHYS_LIMIT-PAGE)
            return TIS782_LAYOUT;
        const struct boot_range r={start,start+PAGE};
        if(!contains(e->live.image,r)) return TIS782_LAYOUT;
        bool reserved=false;
        for(size_t j=0;j<m->nr_reserved;++j)
            if(contains(m->reserved[j],r)) reserved=true;
        if(!reserved) return TIS782_NOT_RESERVED;
        for(size_t j=0;j<m->nr_candidates;++j)
            if(overlap(m->candidates[j],r)) return TIS782_CONFLICT;
        for(unsigned j=0;j<sizeof(protected)/sizeof(protected[0]);++j)
            if(valid_range(protected[j]) && overlap(protected[j],r))
                return TIS782_CONFLICT;
        if(overlap(t,r)) return TIS782_CONFLICT;
        for(unsigned a=0;a<DVM_USER_SLOTS;++a)
            for(unsigned b=0;b<DVM_USER_PAGES;++b) {
                const struct boot_range user={prior->user[a][b],prior->user[a][b]+PAGE};
                if(overlap(r,user)) return TIS782_CONFLICT;
            }
        for(unsigned j=0;j<i;++j)
            if(p[j]==start) return TIS782_CONFLICT;
    }
    /* Attestation is always after COMPLETE structural checking. No partial
     * publication or CPU writes are possible if any frame fails to walk. */
    for(unsigned i=0;i<4u;++i)
        if(!attest(context,p[i],PAGE)) return TIS782_UNATTESTED;
    *out=(struct tis782_plan){p[2]+PAGE-16u,p[3]+PAGE-16u,true,false};
    return TIS782_OK;
}
static void put64(uint8_t *p,uint64_t value)
{ for(unsigned i=0;i<8u;++i)p[i]=(uint8_t)(value>>(i*8u)); }
bool tis782_encode(const struct tis782_plan *p,uint8_t *bytes,size_t size)
{
    if(!p || !p->staged || p->ltr_permitted || !p->rsp0_top || !p->ist1_top ||
       (p->rsp0_top&15u) || (p->ist1_top&15u) || !bytes ||
       size!=TIS782_TSS_SIZE) return false;
    for(size_t i=0;i<size;++i)bytes[i]=0;
    put64(bytes+4u,p->rsp0_top); /* x86-64 TSS.RSP0 */
    put64(bytes+36u,p->ist1_top); /* x86-64 TSS.IST1 */
    bytes[102u]=(uint8_t)TIS782_TSS_SIZE; /* I/O map beyond TSS limit */
    return true;
}
