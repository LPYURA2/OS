#include <styleos/user_root_846.h>
#define P UINT64_C(4096)
#define LOW (UINT64_C(1)<<47)
#define NX (UINT64_C(1)<<63)
static const uint64_t user_va[3]={S846_CODE,S846_DATA,S846_STACK};
static bool overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m){return a<b+m&&b<a+n;}
static bool match(struct s844_lease a,struct s844_lease b){return a.pa==b.pa&&a.generation==b.generation&&a.owner==b.owner&&a.pages==b.pages;}
static bool range(uint64_t x,uint64_t n){return x&&x<LOW&&n<=LOW-x;}
static bool ptr_range(const void *x,size_t n){return x&&n<=UINTPTR_MAX-(uintptr_t)x;}
static bool separate(const void *a,size_t n,const void *b,size_t m){return ptr_range(a,n)&&ptr_range(b,m)&&!overlap((uintptr_t)a,n,(uintptr_t)b,m);}
/* Bounds match native_app_image_75.h; the old null-layout path retains
 * exactly three pages. All three user ranges stay within their respective
 * 2 MiB subtree, so existing ancestor U/S classification remains exact. */
static struct s846_extent extent(const struct s846_extent *e,unsigned i){
 return e?e[i]:(struct s846_extent){1,1,1};
}
static bool extent_ok(struct s846_extent e){
 return e.code&&e.code<=8&&e.data&&e.data<=16&&e.stack&&e.stack<=4;
}
static bool owner_ok(const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,bool sparse,const struct s846_extent *extents){
 unsigned count,total=0;
 if(!a||!l||id>=S846_ROOT_COUNT||!s844_count(a,&count)||
    (extents&&(!ptr_range(extents,S846_ROOT_COUNT*sizeof *extents)||
      (uintptr_t)extents%_Alignof(struct s846_extent)||
      !separate(extents,S846_ROOT_COUNT*sizeof *extents,a,sizeof *a)||
      !separate(extents,S846_ROOT_COUNT*sizeof *extents,l,S846_ROOT_COUNT*sizeof *l)||
      !separate(extents,S846_ROOT_COUNT*sizeof *extents,(void *)(uintptr_t)a->base,S844_BYTES))))return false;
 for(unsigned i=0;i<S846_ROOT_COUNT;i++){
  struct s844_lease v=l[i];
  struct s846_extent e=extent(extents,i);
  if(!v.generation){
   if(!sparse||i==id||v.pa||v.owner||v.pages||(extents&&(e.code||e.data||e.stack)))return false;
   continue;
  }
  if(!extent_ok(e)||v.pages!=e.code+e.data+e.stack||v.pages>S844_PAGES||
     v.owner!=S846_OWNER_A+i||v.pa<a->base||
     v.pa>a->base+S844_BYTES-v.pages*P||(v.pa%P)||
     !match(v,a->slots[(v.pa-a->base)/P]))return false;
  total+=v.pages;
 }
 for(unsigned i=0;i<S846_ROOT_COUNT;i++)for(unsigned j=0;j<i;j++)
  if(l[i].generation&&l[j].generation&&overlap(l[i].pa,l[i].pages*P,l[j].pa,l[j].pages*P))return false;
 return count==total;
}
/* Reconstruct an exact expected plan: omit the ENTIRE arena identity extent,
 * retain all supervisor leaves, and map only the current lease at user VAs. */
static bool plan(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,uint64_t (*tables)[512],uint64_t pa,struct pg_plan *p,bool sparse,const struct s846_extent *extents){
 if(id>=S846_ROOT_COUNT||!safe||!c||!owner_ok(a,l,id,sparse,extents)||!s842_audit(safe,c)||!tables||((uintptr_t)tables%P)||pa%P||!range(pa,PG_TABLE_BYTES)||!separate(tables,PG_TABLE_BYTES,safe->plan.tables,safe->plan.capacity*P)||overlap(pa,PG_TABLE_BYTES,safe->plan.tables_pa,safe->plan.capacity*P)||overlap(pa,PG_TABLE_BYTES,a->base,S844_BYTES))return false;
 if(!separate(tables,PG_TABLE_BYTES,safe,sizeof *safe)||!separate(tables,PG_TABLE_BYTES,c,sizeof *c)||!separate(tables,PG_TABLE_BYTES,a,sizeof *a)||!separate(tables,PG_TABLE_BYTES,l,S846_ROOT_COUNT*sizeof *l)||!separate(tables,PG_TABLE_BYTES,(void *)(uintptr_t)a->base,S844_BYTES))return false;
 if(extents&&!separate(extents,S846_ROOT_COUNT*sizeof *extents,tables,PG_TABLE_BYTES))return false;
 *p=(struct pg_plan){.arch=PG_X86,.pa_bits=c->bits,.tables=tables,.tables_pa=pa,.capacity=PG_TABLE_PAGES,.probe=safe->plan.probe};
 unsigned removed=0;bool covered=false;
 for(size_t i=0;i<safe->plan.count;i++){
  struct pg_mapping m=safe->plan.maps[i];
  if(overlap(m.pa,m.bytes,a->base,S844_BYTES)){
   if(m.pa!=a->base||m.va!=a->base||m.bytes!=S844_BYTES||m.flags!=PG_WRITE)return false;
   removed++;continue;
  }
  if(p->count>=PG_MAX_MAPS-3)return false;
  if(m.flags==PG_WRITE&&m.pa<=pa&&pa+PG_TABLE_BYTES<=m.pa+m.bytes&&m.pa!=safe->lapic&&m.pa!=safe->ioapic)covered=true;
  p->maps[p->count++]=m;
 }
 if(removed!=1||!covered)return false;
 if(g){
  if(p->count>PG_MAX_MAPS-6||!separate(g,sizeof *g,tables,PG_TABLE_BYTES))return false;
  for(unsigned i=0;i<3;i++){
   uint64_t x=g->pa[i],va=S846_IST+i*3*P;bool rw=false;
   if(!range(x,P)||x%P||overlap(x,P,pa,PG_TABLE_BYTES)||overlap(x,P,safe->plan.tables_pa,safe->plan.capacity*P)||overlap(x,P,a->base,S844_BYTES))return false;
   for(unsigned j=0;j<i;j++)if(x==g->pa[j])return false;
   for(size_t j=0;j<p->count;j++){
    struct pg_mapping m=p->maps[j];
    if(m.va==m.pa&&m.flags==PG_WRITE&&m.pa<=x&&x-m.pa<m.bytes&&x!=safe->lapic&&x!=safe->ioapic)rw=true;
    if(overlap(va-P,3*P,m.va,m.bytes))return false;
   }
   if(!rw)return false;
   p->maps[p->count++]=(struct pg_mapping){va,x,P,PG_WRITE};
  }
 }
 struct s846_extent e=extent(extents,id);
 const unsigned pages[3]={e.code,e.data,e.stack};
 for(size_t j=0;j<p->count;j++)if(overlap(S846_STACK-P,(e.stack+2u)*P,p->maps[j].va,p->maps[j].bytes))return false;
 uint64_t physical=l[id].pa;
 for(unsigned i=0;i<3;i++){
  uint64_t bytes=pages[i]*P;
  for(size_t j=0;j<p->count;j++)if(overlap(user_va[i],bytes,p->maps[j].va,p->maps[j].bytes))return false;
  p->maps[p->count++]=(struct pg_mapping){user_va[i],physical,bytes,i?PG_WRITE:PG_EXEC};
  physical+=bytes;
 }
 return true;
}
static uint64_t mask(const struct pg_plan *p){return ((UINT64_C(1)<<p->pa_bits)-1)&~(P-1);}
static uint64_t cache_bits(unsigned i){return ((i&1)?8u:0u)|((i&2)?16u:0u)|((i&4)?128u:0u);}
static uint64_t *leaf(const struct pg_plan *p,size_t used,uint64_t va,bool user){
 size_t t=0;uint64_t m=mask(p);
 for(unsigned level=4;level>1;level--){uint64_t *e=&p->tables[t][(va>>(12+9*(level-1)))&511u];uint64_t pa=*e&m;
  if((*e&~(m|4u))!=3||pa<p->tables_pa||pa-p->tables_pa>=used*P)return 0;
  if(user)*e|=4;
  t=(size_t)((pa-p->tables_pa)/P);
 }
 return &p->tables[t][(va>>12)&511u];
}
static bool user_subtree(uint64_t prefix,unsigned shift){for(unsigned i=0;i<3;i++)if((user_va[i]>>shift)==(prefix>>shift))return true;return false;}
static bool expected(const struct pg_plan *p,const struct s842_root *safe,uint64_t va,uint64_t *want){
 for(size_t i=0;i<p->count;i++){
  struct pg_mapping m=p->maps[i];
  if(va<m.va||va-m.va>=m.bytes)continue;
  uint64_t pa=m.pa+va-m.va;
  *want=pa|1u|((m.flags&PG_WRITE)?2u:0u)|((m.flags&PG_EXEC)?0:NX);
  if(i>=p->count-3)*want|=4;
  if(pa==safe->lapic||pa==safe->ioapic)*want|=cache_bits(safe->uc_index);
  return true;
 }
 return false;
}
static bool walk(const struct pg_plan *p,const struct s842_root *safe,size_t used,size_t t,unsigned level,uint64_t prefix,uint32_t *seen,uint64_t *leaves){
 if(t>=used||(*seen&(UINT32_C(1)<<t)))return false;
 *seen|=UINT32_C(1)<<t;
 uint64_t m=mask(p);unsigned shift=12+9*(level-1);
 for(unsigned i=0;i<512;i++){
  uint64_t e=p->tables[t][i],va=prefix|((uint64_t)i<<shift);
  if(!e)continue;
  if(level==1){uint64_t want;if(!expected(p,safe,va,&want)||(e&~UINT64_C(0x60))!=want)return false;(*leaves)++;}
  else {uint64_t pa=e&m,bits=3u|(user_subtree(va,shift)?4u:0u);
   if((e&~(m|UINT64_C(0x20)))!=bits||pa<p->tables_pa||pa-p->tables_pa>=used*P||!walk(p,safe,used,(size_t)((pa-p->tables_pa)/P),level-1,va,seen,leaves))return false;
  }
 }
 return true;
}
static bool same_plan(const struct pg_plan *a,const struct pg_plan *b){
 if(a->arch!=b->arch||a->pa_bits!=b->pa_bits||a->tables!=b->tables||a->tables_pa!=b->tables_pa||a->capacity!=b->capacity||a->count!=b->count||a->probe!=b->probe)return false;
 for(size_t i=0;i<a->count;i++)if(a->maps[i].va!=b->maps[i].va||a->maps[i].pa!=b->maps[i].pa||a->maps[i].bytes!=b->maps[i].bytes||a->maps[i].flags!=b->maps[i].flags)return false;
 return true;
}
static bool audit_root(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,const struct s846_root *r,bool sparse,const struct s846_extent *extents){
 struct pg_plan p;
 if(!r||!r->ready||r->built.error!=PG_OK||!r->built.used||r->built.used>PG_TABLE_PAGES||r->built.root!=r->plan.tables_pa||!plan(safe,c,a,l,id,g,r->plan.tables,r->plan.tables_pa,&p,sparse,extents)||!same_plan(&p,&r->plan))return false;
#if (defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
 if(!match(r->lease,l[id]))return false;
#endif
 uint32_t seen=0;uint64_t leaves=0,n=0;for(size_t i=0;i<p.count;i++)n+=p.maps[i].bytes/P;
 return walk(&p,safe,r->built.used,0,4,0,&seen,&leaves)&&seen==(r->built.used==32?UINT32_MAX:(UINT32_C(1)<<r->built.used)-1)&&leaves==n;
}
static bool build_root(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,uint64_t (*tables)[512],uint64_t pa,struct s846_root *out,bool sparse,const struct s846_extent *extents){
 if(!out||!safe||!c||!a||!l||!separate(out,sizeof *out,safe,sizeof *safe)||!separate(out,sizeof *out,c,sizeof *c)||!separate(out,sizeof *out,a,sizeof *a)||!separate(out,sizeof *out,l,S846_ROOT_COUNT*sizeof *l)||!separate(out,sizeof *out,tables,PG_TABLE_BYTES)||!separate(out,sizeof *out,safe->plan.tables,safe->plan.capacity*P)||!separate(out,sizeof *out,(void *)(uintptr_t)a->base,S844_BYTES)||(g&&!separate(out,sizeof *out,g,sizeof *g))||out->ready)return false;
 if(extents&&!separate(out,sizeof *out,extents,S846_ROOT_COUNT*sizeof *extents))return false;
 struct s846_root r={0};
 if(!plan(safe,c,a,l,id,g,tables,pa,&r.plan,sparse,extents))return false;
 r.built=pg_build(&r.plan);if(r.built.error!=PG_OK)return false;
 for(size_t i=r.plan.count-3;i<r.plan.count;++i)
  for(uint64_t off=0;off<r.plan.maps[i].bytes;off+=P){
   uint64_t *e=leaf(&r.plan,r.built.used,r.plan.maps[i].va+off,true);
   if(!e)return false;
   *e|=4;
  }
 uint64_t dev[2]={safe->lapic,safe->ioapic};
 for(unsigned i=0;i<2;i++){uint64_t *e=leaf(&r.plan,r.built.used,dev[i],false);if(!e)return false;*e|=cache_bits(safe->uc_index);}
#if (defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
 r.lease=l[id];
#endif
 r.ready=true;if(!audit_root(safe,c,a,l,id,g,&r,sparse,extents))return false;
 *out=r;return true;
}

bool s846_audit(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,const struct s846_root *r){
 return audit_root(safe,c,a,l,id,g,r,false,0);
}
bool s846_build(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,uint64_t (*tables)[512],uint64_t pa,struct s846_root *out){
 return build_root(safe,c,a,l,id,g,tables,pa,out,false,0);
}
#if (defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
bool s865_root_audit(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,const struct s846_root *r){
 return audit_root(safe,c,a,l,id,g,r,true,0);
}
bool s865_root_build(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,uint64_t (*tables)[512],uint64_t pa,struct s846_root *out){
 return build_root(safe,c,a,l,id,g,tables,pa,out,true,0);
}
bool s875_root_audit(const struct s842_root *safe,const struct s842_cache *c,
 const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],
 const struct s846_extent extents[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,const struct s846_root *r){
 return extents&&audit_root(safe,c,a,l,id,g,r,true,extents);
}
bool s875_root_build(const struct s842_root *safe,const struct s842_cache *c,
 const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],
 const struct s846_extent extents[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,
 uint64_t (*tables)[512],uint64_t pa,struct s846_root *out){
 return extents&&build_root(safe,c,a,l,id,g,tables,pa,out,true,extents);
}
uint64_t s875_data_pa(const struct s844_arena *a,struct s844_lease lease,
 struct s846_extent e,uint64_t va,size_t bytes){
 if(!extent_ok(e)||lease.pages!=e.code+e.data+e.stack||
    !s844_lease_live(a,lease)||!bytes||va<S846_DATA)return 0;
 uint64_t length=e.data*P,offset=va-S846_DATA;
 if(bytes>length||offset>length-bytes)return 0;
 return lease.pa+e.code*P+offset;
}
#endif

#if !(defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
bool s846_audit_pair(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *arena,const struct s844_lease l[S846_ROOT_COUNT],const struct s846_guard *g,const struct s846_root *a,const struct s846_root *b){
 if(!separate(a,sizeof *a,b,sizeof *b)||!s846_audit(safe,c,arena,l,0,g,a)||!s846_audit(safe,c,arena,l,1,g,b))return false;
 return separate(a->plan.tables,PG_TABLE_BYTES,b->plan.tables,PG_TABLE_BYTES)&&
 !overlap(a->plan.tables_pa,PG_TABLE_BYTES,b->plan.tables_pa,PG_TABLE_BYTES)&&
 separate(a,sizeof *a,b->plan.tables,PG_TABLE_BYTES)&&separate(b,sizeof *b,a->plan.tables,PG_TABLE_BYTES);
}
#else
bool s846_audit_set(const struct s842_root *safe,const struct s842_cache *c,
 const struct s844_arena *arena,const struct s844_lease l[S846_ROOT_COUNT],
 const struct s846_guard *g,const struct s846_root roots[S846_ROOT_COUNT])
{
 if(!roots||!ptr_range(roots,S846_ROOT_COUNT*sizeof *roots))return false;
 for(unsigned i=0;i<S846_ROOT_COUNT;i++){
  if(!s846_audit(safe,c,arena,l,i,g,&roots[i])||
     !separate(roots,S846_ROOT_COUNT*sizeof *roots,roots[i].plan.tables,PG_TABLE_BYTES))return false;
  for(unsigned j=0;j<i;j++)
   if(!separate(roots[i].plan.tables,PG_TABLE_BYTES,roots[j].plan.tables,PG_TABLE_BYTES)||
      overlap(roots[i].plan.tables_pa,PG_TABLE_BYTES,roots[j].plan.tables_pa,PG_TABLE_BYTES))return false;
 }
 return true;
}
#endif
