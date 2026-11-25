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
static bool owner_ok(const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT]){
 unsigned count;
 if(!a||!l||!s844_count(a,&count)||count!=3*S846_ROOT_COUNT)return false;
 for(unsigned i=0;i<S846_ROOT_COUNT;i++){
  struct s844_lease v=l[i];
  if(v.pages!=3||v.owner!=S846_OWNER_A+i||!v.generation||v.pa<a->base||v.pa>a->base+S844_BYTES-3*P||(v.pa%P)||!match(v,a->slots[(v.pa-a->base)/P]))return false;
 }
 for(unsigned i=0;i<S846_ROOT_COUNT;i++)for(unsigned j=0;j<i;j++)
  if(overlap(l[i].pa,3*P,l[j].pa,3*P))return false;
 return true;
}
/* Reconstruct an exact expected plan: omit the ENTIRE arena identity extent,
 * retain all supervisor leaves, and map only the current lease at user VAs. */
static bool plan(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,uint64_t (*tables)[512],uint64_t pa,struct pg_plan *p){
 if(id>=S846_ROOT_COUNT||!safe||!c||!owner_ok(a,l)||!s842_audit(safe,c)||!tables||((uintptr_t)tables%P)||pa%P||!range(pa,PG_TABLE_BYTES)||!separate(tables,PG_TABLE_BYTES,safe->plan.tables,safe->plan.capacity*P)||overlap(pa,PG_TABLE_BYTES,safe->plan.tables_pa,safe->plan.capacity*P)||overlap(pa,PG_TABLE_BYTES,a->base,S844_BYTES))return false;
 if(!separate(tables,PG_TABLE_BYTES,safe,sizeof *safe)||!separate(tables,PG_TABLE_BYTES,c,sizeof *c)||!separate(tables,PG_TABLE_BYTES,a,sizeof *a)||!separate(tables,PG_TABLE_BYTES,l,S846_ROOT_COUNT*sizeof *l)||!separate(tables,PG_TABLE_BYTES,(void *)(uintptr_t)a->base,S844_BYTES))return false;
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
 for(size_t j=0;j<p->count;j++)if(overlap(S846_STACK-P,3*P,p->maps[j].va,p->maps[j].bytes))return false;
 for(unsigned i=0;i<3;i++){
  for(size_t j=0;j<p->count;j++)if(overlap(user_va[i],P,p->maps[j].va,p->maps[j].bytes))return false;
  p->maps[p->count++]=(struct pg_mapping){user_va[i],l[id].pa+i*P,P,i?PG_WRITE:PG_EXEC};
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
bool s846_audit(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,const struct s846_root *r){
 struct pg_plan p;
 if(!r||!r->ready||r->built.error!=PG_OK||!r->built.used||r->built.used>PG_TABLE_PAGES||r->built.root!=r->plan.tables_pa||!plan(safe,c,a,l,id,g,r->plan.tables,r->plan.tables_pa,&p)||!same_plan(&p,&r->plan))return false;
#ifdef STYLEOS_TEST_WIDE_ROOTS_864
 if(!match(r->lease,l[id]))return false;
#endif
 uint32_t seen=0;uint64_t leaves=0,n=0;for(size_t i=0;i<p.count;i++)n+=p.maps[i].bytes/P;
 return walk(&p,safe,r->built.used,0,4,0,&seen,&leaves)&&seen==(r->built.used==32?UINT32_MAX:(UINT32_C(1)<<r->built.used)-1)&&leaves==n;
}
bool s846_build(const struct s842_root *safe,const struct s842_cache *c,const struct s844_arena *a,const struct s844_lease l[S846_ROOT_COUNT],unsigned id,const struct s846_guard *g,uint64_t (*tables)[512],uint64_t pa,struct s846_root *out){
 if(!out||!safe||!c||!a||!l||!separate(out,sizeof *out,safe,sizeof *safe)||!separate(out,sizeof *out,c,sizeof *c)||!separate(out,sizeof *out,a,sizeof *a)||!separate(out,sizeof *out,l,S846_ROOT_COUNT*sizeof *l)||!separate(out,sizeof *out,tables,PG_TABLE_BYTES)||!separate(out,sizeof *out,safe->plan.tables,safe->plan.capacity*P)||!separate(out,sizeof *out,(void *)(uintptr_t)a->base,S844_BYTES)||(g&&!separate(out,sizeof *out,g,sizeof *g))||out->ready)return false;
 struct s846_root r={0};
 if(!plan(safe,c,a,l,id,g,tables,pa,&r.plan))return false;
 r.built=pg_build(&r.plan);if(r.built.error!=PG_OK)return false;
 for(unsigned i=0;i<3;i++){uint64_t *e=leaf(&r.plan,r.built.used,user_va[i],true);if(!e)return false;*e|=4;}
 uint64_t dev[2]={safe->lapic,safe->ioapic};
 for(unsigned i=0;i<2;i++){uint64_t *e=leaf(&r.plan,r.built.used,dev[i],false);if(!e)return false;*e|=cache_bits(safe->uc_index);}
#ifdef STYLEOS_TEST_WIDE_ROOTS_864
 r.lease=l[id];
#endif
 r.ready=true;if(!s846_audit(safe,c,a,l,id,g,&r))return false;
 *out=r;return true;
}

#ifndef STYLEOS_TEST_WIDE_ROOTS_864
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
