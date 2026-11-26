#include <styleos/stylefs_alloc64.h>

static bool valid(uint64_t cap,uint64_t start,struct sfs64_inventory inv)
{
    if(inv.count>SIZE_MAX/sizeof(struct sfs64_run)||(inv.count&&!inv.runs))return false;
    uint64_t end=start;
    for(size_t i=0;i<inv.count;++i){
        struct sfs64_run r=inv.runs[i];
        if(!r.sectors||r.first<end||r.first>=cap||r.sectors>cap-r.first)return false;
        end=r.first+r.sectors;
    }
    return true;
}
static bool geometry(uint64_t cap,uint64_t start,struct sfs64_inventory a,struct sfs64_inventory b)
{return start&&start<cap&&valid(cap,start,a)&&valid(cap,start,b);}

struct scan {
    uint64_t cursor,capacity;
    struct sfs64_inventory a,b;
    size_t ia,ib;
};
/* Merge the sorted occupied lists, coalescing overlaps and adjacency.
 * Yield only actual holes. No sector-by-sector scan or per-sector bitmap. */
static bool gap(struct scan *s,struct sfs64_run *out)
{
    while(s->ia<s->a.count||s->ib<s->b.count){
        bool take_a=s->ib==s->b.count||
            (s->ia<s->a.count&&s->a.runs[s->ia].first<=s->b.runs[s->ib].first);
        struct sfs64_run r=take_a?s->a.runs[s->ia]:s->b.runs[s->ib];
        if(r.first>s->cursor){
            *out=(struct sfs64_run){s->cursor,r.first-s->cursor};
            s->cursor=r.first;return true;
        }
        if(take_a)++s->ia;else ++s->ib;
        uint64_t end=r.first+r.sectors;
        if(end>s->cursor)s->cursor=end;
    }
    if(s->cursor<s->capacity){
        *out=(struct sfs64_run){s->cursor,s->capacity-s->cursor};
        s->cursor=s->capacity;return true;
    }
    return false;
}
enum sfs64_alloc_status sfs64_space_query(uint64_t cap,uint64_t start,
    struct sfs64_inventory a,struct sfs64_inventory b,struct sfs64_space *out)
{
    if(!out||!geometry(cap,start,a,b))return SFS64_ALLOC_INVALID;
    struct scan s={start,cap,a,b,0,0};
    struct sfs64_space total={0};struct sfs64_run r;
    while(gap(&s,&r)){
        if(total.gaps==SIZE_MAX)return SFS64_ALLOC_INVALID;
        ++total.gaps;total.free_sectors+=r.sectors;
        if(r.sectors>total.largest_run)total.largest_run=r.sectors;
    }
    *out=total;return SFS64_ALLOC_OK;
}
enum sfs64_alloc_status sfs64_allocate(uint64_t cap,uint64_t start,
    struct sfs64_inventory a,struct sfs64_inventory b,uint64_t bytes,
    struct sfs64_run *out,size_t slots,size_t *count)
{
    if(!count||(!out&&slots)||slots>SIZE_MAX/sizeof *out||!geometry(cap,start,a,b))
        return SFS64_ALLOC_INVALID;
    uint64_t need=sfs64_sectors(bytes),left=need;
    struct scan s={start,cap,a,b,0,0};struct sfs64_run r;
    size_t required=0;
    while(left&&gap(&s,&r)){
        if(required==SIZE_MAX)return SFS64_ALLOC_INVALID;
        ++required;left-=r.sectors<left?r.sectors:left;
    }
    if(left)return SFS64_ALLOC_FULL;
    if(required>slots){*count=required;return SFS64_ALLOC_OUTPUT;}
    /* Dry run succeeded; immutable trusted inventories guarantee the same
     * second pass. Nothing is reserved until the transaction owns this plan. */
    s=(struct scan){start,cap,a,b,0,0};left=need;
    for(size_t i=0;i<required;++i){
        if(!gap(&s,&r))return SFS64_ALLOC_INVALID;
        if(r.sectors>left)r.sectors=left;
        out[i]=r;left-=r.sectors;
    }
    *count=required;return SFS64_ALLOC_OK;
}
