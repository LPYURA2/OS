#include <styleos/runtime_roots_861.h>
#define P UINT64_C(4096)
static bool apart(uint64_t a,uint64_t n,uint64_t b,uint64_t m){
 return a&&b&&n&&m&&n<=UINT64_MAX-a&&m<=UINT64_MAX-b&&(a+n<=b||b+m<=a);
}
static bool tables_apart(const struct pg_plan *a,const struct pg_plan *b){
 return apart(a->tables_pa,a->capacity*P,b->tables_pa,b->capacity*P)&&
        apart((uintptr_t)a->tables,a->capacity*P,(uintptr_t)b->tables,b->capacity*P);
}
static bool cross(const struct s844_arena *arena,const struct s850_desktop_root *desktop,const struct s846_root *a,const struct s846_root *b){
 const struct pg_plan *d=&desktop->plan;
 if(!tables_apart(d,&a->plan)||!tables_apart(d,&b->plan)||
    !apart(d->tables_pa,d->capacity*P,arena->base,S844_BYTES)||
    !apart((uintptr_t)d->tables,d->capacity*P,arena->base,S844_BYTES))return false;
 /* Refuse a GOP physical or virtual overlap with any child mapping,
  * including the user code/data/stack virtual addresses. No extra leaves
  * are possible after the existing full audits. */
 const struct pg_mapping *g=&desktop->gop.mapping;
 const struct pg_plan *u[2]={&a->plan,&b->plan};
 for(unsigned i=0;i<2;i++)for(size_t j=0;j<u[i]->count;j++){
  const struct pg_mapping *m=&u[i]->maps[j];
  if(!apart(g->pa,g->bytes,m->pa,m->bytes)||!apart(g->va,g->bytes,m->va,m->bytes))return false;
 }
 return true;
}

bool rt861_roots_audit(const struct s842_root *base,const struct s850_gop_request *req,
 const struct s844_arena *arena,const struct s844_lease leases[2],const struct s846_guard *guard,
 const struct s850_desktop_root *desktop,const struct s846_root *a,const struct s846_root *b){
 if(!base||!req||!arena||!leases||!guard||!desktop||!a||!b)return false;
 /* Reuse the complete existing tree/cache/lease audits with identical base
  * and guard evidence; do not introduce another page table builder. */
 if(!s850_desktop_audit_guarded(base,req,guard,desktop)||
    !s846_audit_pair(base,req->cache,arena,leases,guard,a,b))return false;
 return cross(arena,desktop,a,b);
}

#ifdef STYLEOS_TEST_RUNTIME_ROOTS_861
bool rt861_roots_audit_frozen(const struct s842_root *base,const struct s842_cache *cache,const struct s850_gop_plan *proof,const struct boot_memory_evidence *e,const struct s844_arena *arena,const struct s844_lease leases[2],const struct s846_guard *guard,const struct s850_desktop_root *desktop,const struct s846_root *a,const struct s846_root *b){
 return base&&cache&&proof&&e&&arena&&leases&&guard&&desktop&&a&&b&&
  s850_desktop_audit_frozen_861(base,cache,proof,e,guard,desktop)&&s846_audit_pair(base,cache,arena,leases,guard,a,b)&&cross(arena,desktop,a,b);
}
#endif
