#include <styleos/runtime_roots_861.h>
#define P UINT64_C(4096)
static bool apart(uint64_t a,uint64_t n,uint64_t b,uint64_t m){
 return a&&b&&n&&m&&n<=UINT64_MAX-a&&m<=UINT64_MAX-b&&(a+n<=b||b+m<=a);
}
static bool tables_apart(const struct pg_plan *a,const struct pg_plan *b){
 return apart(a->tables_pa,a->capacity*P,b->tables_pa,b->capacity*P)&&
        apart((uintptr_t)a->tables,a->capacity*P,(uintptr_t)b->tables,b->capacity*P);
}
static bool cross(const struct s844_arena *arena,const struct s850_desktop_root *desktop,const struct s846_root *const *roots,unsigned n){
 const struct pg_plan *d=&desktop->plan;
 for(unsigned i=0;i<n;i++)if(!tables_apart(d,&roots[i]->plan))return false;
 if(!apart(d->tables_pa,d->capacity*P,arena->base,S844_BYTES)||
    !apart((uintptr_t)d->tables,d->capacity*P,arena->base,S844_BYTES))return false;
 /* Refuse a GOP physical or virtual overlap with any child mapping,
  * including the user code/data/stack virtual addresses. No extra leaves
  * are possible after the existing full audits. */
 const struct pg_mapping *g=&desktop->gop.mapping;
 for(unsigned i=0;i<n;i++)for(size_t j=0;j<roots[i]->plan.count;j++){
  const struct pg_mapping *m=&roots[i]->plan.maps[j];
  if(!apart(g->pa,g->bytes,m->pa,m->bytes)||!apart(g->va,g->bytes,m->va,m->bytes))return false;
 }
 return true;
}

#if (defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
/* Validate every currently allocated page and every published root, without
 * requiring unrelated slots to exist. Vacant slots have zero metadata.
 * The trusted architecture owns the safe root and serializes changes. */
static bool sparse(const struct s842_root *base,const struct s842_cache *cache,
 const struct s844_arena *arena,const struct s844_lease leases[S846_ROOT_COUNT],
 const struct s846_guard *guard,const struct s850_desktop_root *desktop,
 const struct s846_root roots[S846_ROOT_COUNT]){
 if(!base||!cache||!arena||!leases||!guard||!desktop||!roots)return false;
 unsigned used,n=0;
 if(!s844_count(arena,&used))return false;
 const struct s846_root *set[S846_ROOT_COUNT];
 for(unsigned i=0;i<S846_ROOT_COUNT;i++){
  if(!leases[i].generation){
   if(leases[i].pa||leases[i].owner||leases[i].pages)return false;
   const unsigned char *p=(const void *)&roots[i];
   for(size_t j=0;j<sizeof roots[i];j++)if(p[j])return false;
   continue;
  }
  if(!s865_root_audit(base,cache,arena,leases,i,guard,&roots[i])||
     !apart((uintptr_t)roots,S846_ROOT_COUNT*sizeof *roots,
            (uintptr_t)roots[i].plan.tables,PG_TABLE_BYTES))return false;
  for(unsigned j=0;j<n;j++)if(!tables_apart(&roots[i].plan,&set[j]->plan))return false;
  set[n++]=&roots[i];
 }
 return used==3*n&&cross(arena,desktop,set,n);
}
bool rt865_roots_audit_sparse(const struct s842_root *base,const struct s850_gop_request *req,
 const struct s844_arena *arena,const struct s844_lease leases[S846_ROOT_COUNT],
 const struct s846_guard *guard,const struct s850_desktop_root *desktop,
 const struct s846_root roots[S846_ROOT_COUNT]){
 return base&&req&&arena&&leases&&guard&&desktop&&roots&&
  s850_desktop_audit_guarded(base,req,guard,desktop)&&sparse(base,req->cache,arena,leases,guard,desktop,roots);
}
bool rt864_roots_audit_set(const struct s842_root *base,const struct s850_gop_request *req,
 const struct s844_arena *arena,const struct s844_lease leases[S846_ROOT_COUNT],
 const struct s846_guard *guard,const struct s850_desktop_root *desktop,
 const struct s846_root roots[S846_ROOT_COUNT]){
 if(!base||!req||!arena||!leases||!guard||!desktop||!roots||
    !s850_desktop_audit_guarded(base,req,guard,desktop)||
    !s846_audit_set(base,req->cache,arena,leases,guard,roots))return false;
 const struct s846_root *set[S846_ROOT_COUNT];
 for(unsigned i=0;i<S846_ROOT_COUNT;i++)set[i]=&roots[i];
 return cross(arena,desktop,set,S846_ROOT_COUNT);
}
#if (defined(STYLEOS_TEST_FOUR_GUEST_864) || defined(STYLEOS_OWNED_SHELL_865))
bool rt865_roots_audit_sparse_frozen(const struct s842_root *base,const struct s842_cache *cache,const struct s850_gop_plan *proof,const struct boot_memory_evidence *e,const struct s844_arena *arena,const struct s844_lease leases[S846_ROOT_COUNT],const struct s846_guard *guard,const struct s850_desktop_root *desktop,const struct s846_root roots[S846_ROOT_COUNT]){
 return base&&cache&&proof&&e&&arena&&leases&&guard&&desktop&&roots&&
  s850_desktop_audit_frozen_861(base,cache,proof,e,guard,desktop)&&sparse(base,cache,arena,leases,guard,desktop,roots);
}
bool rt864_roots_audit_frozen(const struct s842_root *base,const struct s842_cache *cache,const struct s850_gop_plan *proof,const struct boot_memory_evidence *e,const struct s844_arena *arena,const struct s844_lease leases[S846_ROOT_COUNT],const struct s846_guard *guard,const struct s850_desktop_root *desktop,const struct s846_root roots[S846_ROOT_COUNT]){
 if(!base||!cache||!proof||!e||!arena||!leases||!guard||!desktop||!roots||
    !s850_desktop_audit_frozen_861(base,cache,proof,e,guard,desktop)||
    !s846_audit_set(base,cache,arena,leases,guard,roots))return false;
 const struct s846_root *set[S846_ROOT_COUNT];
 for(unsigned i=0;i<S846_ROOT_COUNT;i++)set[i]=&roots[i];
 return cross(arena,desktop,set,S846_ROOT_COUNT);
}
#endif
#else
bool rt861_roots_audit(const struct s842_root *base,const struct s850_gop_request *req,
 const struct s844_arena *arena,const struct s844_lease leases[2],const struct s846_guard *guard,
 const struct s850_desktop_root *desktop,const struct s846_root *a,const struct s846_root *b){
 if(!base||!req||!arena||!leases||!guard||!desktop||!a||!b)return false;
 /* Reuse the complete existing tree/cache/lease audits with identical base
  * and guard evidence; do not introduce another page table builder. */
 if(!s850_desktop_audit_guarded(base,req,guard,desktop)||
    !s846_audit_pair(base,req->cache,arena,leases,guard,a,b))return false;
 return cross(arena,desktop,(const struct s846_root *const[]){a,b},2);
}

#if (defined(STYLEOS_TEST_RUNTIME_ROOTS_861) || defined(STYLEOS_OWNED_SHELL_865))
bool rt861_roots_audit_frozen(const struct s842_root *base,const struct s842_cache *cache,const struct s850_gop_plan *proof,const struct boot_memory_evidence *e,const struct s844_arena *arena,const struct s844_lease leases[2],const struct s846_guard *guard,const struct s850_desktop_root *desktop,const struct s846_root *a,const struct s846_root *b){
 return base&&cache&&proof&&e&&arena&&leases&&guard&&desktop&&a&&b&&
  s850_desktop_audit_frozen_861(base,cache,proof,e,guard,desktop)&&s846_audit_pair(base,cache,arena,leases,guard,a,b)&&cross(arena,desktop,(const struct s846_root *const[]){a,b},2);
}
#endif

#endif
