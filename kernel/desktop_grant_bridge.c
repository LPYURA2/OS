#include <styleos/desktop_grant_bridge.h>
#include <styleos/boot_pool.h>

#define DGB_PAGE UINT64_C(4096)
#define DGB_LIMIT (UINT64_C(1) << 52)
static bool overlaps(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a < b + m && b < a + n; }
static bool contains(struct boot_range r,uint64_t pa,uint64_t len)
{ return r.start <= pa && pa <= r.end && len <= r.end - pa; }
static bool kernel_page_ok(const struct boot_memory_evidence *e,
                           const struct fg_claims *c,uint64_t pa)
{
    if(!pa || (pa & (DGB_PAGE-1u)) || pa>DGB_LIMIT-DGB_PAGE ||
       !contains(e->live.image,pa,DGB_PAGE))return false;
    bool reserved=false;
    for(size_t i=0;i<e->map->nr_reserved;++i)
        if(contains(e->map->reserved[i],pa,DGB_PAGE))reserved=true;
    if(!reserved)return false;
    for(size_t i=0;i<e->map->nr_candidates;++i){
        struct boot_range r=e->map->candidates[i];
        if(r.start<r.end && overlaps(pa,DGB_PAGE,r.start,r.end-r.start))return false;
    }
    const struct boot_range excluded[]={e->live.stack,e->live.handoff,
        e->live.source,e->workspace,boot_pool_storage_range(),
        boot_pool_metadata_range()};
    for(size_t i=0;i<sizeof excluded/sizeof excluded[0];++i){
        struct boot_range r=excluded[i];
        if(r.start<r.end && overlaps(pa,DGB_PAGE,r.start,r.end-r.start))return false;
    }
    if(overlaps(pa,DGB_PAGE,c->tables,sizeof(struct dvm_storage)))return false;
    for(unsigned i=0;i<DVM_USER_SLOTS;++i)
        for(unsigned j=0;j<DVM_USER_PAGES;++j)
            if(pa==c->user[i][j])return false;
    return true;
}
static bool map_proof(const struct dfo_owner *o,const struct fg_claims *c,
                      uint64_t kernel_pa)
{
    struct dvm_observed got;
    for(unsigned root=0;root<DVM_ROOTS;++root){
        if(!dvm_lookup(o->storage,&o->roots,root,DGB_SUPERVISOR_VA,&got) ||
           got.pa!=kernel_pa || got.user || !got.writable || got.executable)
            return false;
        for(unsigned p=0;p<DVM_USER_PAGES;++p){
            const uint64_t va=DVM_USER_BEGIN+
                (p==0?0:p==1?0x10000u:0x20000u);
            const bool exists=dvm_lookup(o->storage,&o->roots,root,va,&got);
            if((root!=0)!=exists)return false;
            if(exists && (got.pa!=c->user[root-1u][p] || !got.user ||
               got.writable!=(p!=0) || got.executable!=(p==0)))return false;
        }
    }
    return true;
}
enum dgb_status dgb_prepare(struct dfo_owner *owner,
    struct dvm_storage *storage,const struct boot_memory_evidence *e,
    const struct fg_claims *c,uint64_t supervisor_pa,
    dgb_attest attest,void *context,struct dgb_result *output)
{
    if(!owner || !storage || !e || !c || !attest || !output ||
       (void*)owner==(void*)storage || (void*)owner==(void*)output)
        return DGB_ARGUMENT;
    if(owner->ready)return DGB_READY;
    /* Image test exclusively: no pointer may impersonate a different PA. */
    if((uint64_t)(uintptr_t)storage!=c->tables)return DGB_STORAGE;
    if(fg_check_policy(e,c)!=FG_OK)return DGB_POLICY;
    if(!kernel_page_ok(e,c,supervisor_pa))return DGB_KERNEL;
    if(!attest(context,supervisor_pa,DGB_PAGE) ||
       !attest(context,c->tables,sizeof(struct dvm_storage)))return DGB_ATTEST;
    for(unsigned i=0;i<DVM_USER_SLOTS;++i)
        for(unsigned j=0;j<DVM_USER_PAGES;++j)
            if(!attest(context,c->user[i][j],DGB_PAGE))return DGB_ATTEST;
    struct dfo_claims grants={0};
    grants.table_pa=c->tables;grants.kernel_count=1;
    grants.kernel[0]=(struct dvm_region){
        DGB_SUPERVISOR_VA,supervisor_pa,1,DVM_WRITE};
    for(unsigned i=0;i<DVM_USER_SLOTS;++i)
        for(unsigned j=0;j<DVM_USER_PAGES;++j)
            grants.user_pa[i][j]=c->user[i][j];
    if(dfo_prepare(owner,storage,&grants)!=DFO_OK)return DGB_TABLES;
    if(!map_proof(owner,c,supervisor_pa)){
        /* Never activated; fail closed and scrub the entire detached arena. */
        unsigned char *p=(unsigned char*)storage;
        for(size_t i=0;i<sizeof(*storage);++i)p[i]=0;
        p=(unsigned char*)owner;
        for(size_t i=0;i<sizeof(*owner);++i)p[i]=0;
        return DGB_TABLES;
    }
    *output=(struct dgb_result){owner->roots.root[0],
        {owner->roots.root[1],owner->roots.root[2]},
        DVM_ROOTS*DVM_TABLES_PER_ROOT+DVM_USER_SLOTS*DVM_USER_PAGES+1u};
    return DGB_OK;
}
