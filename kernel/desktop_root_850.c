#include <styleos/desktop_root_850.h>
#define P UINT64_C(4096)
#define NX (UINT64_C(1)<<63)
static bool overlaps(uint64_t a,uint64_t ab,uint64_t b,uint64_t bb){return a<b+bb&&b<a+ab;}
static bool extent(uint64_t a,uint64_t n){return a&&n&&n<=UINT64_MAX-a;}
static bool same_map(const struct pg_mapping *a,const struct pg_mapping *b){return a->va==b->va&&a->pa==b->pa&&a->bytes==b->bytes&&a->flags==b->flags;}
static bool same_gop(const struct s850_gop_plan *a,const struct s850_gop_plan *b){return a->ready&&b->ready&&same_map(&a->mapping,&b->mapping)&&a->cache_bits==b->cache_bits&&a->pages==b->pages&&a->bar_bytes==b->bar_bytes&&a->pat_index==b->pat_index;}
static uint64_t cache_flags(unsigned i){return ((i&1)?8u:0u)|((i&2)?16u:0u)|((i&4)?128u:0u);}
/* Preserve the exact source mapping list. New table storage must already be
 * covered by an admitted writable identity extent, not device pages. */
static bool shape(const struct s842_root *base,const struct s850_gop_plan *g,const struct s846_guard *guard,const struct boot_memory_evidence *e,const struct pg_plan *p){
 if(!base||!g||!g->ready||!p||base->plan.count>=PG_MAX_MAPS||p->count!=base->plan.count+1+(guard?3:0)||p->count>PG_MAX_MAPS||p->arch!=PG_X86||p->arch!=base->plan.arch||p->pa_bits!=base->plan.pa_bits||p->probe!=base->plan.probe||p->capacity!=PG_TABLE_PAGES||!p->tables)return false;
 uint64_t a=p->tables_pa,ptr=(uintptr_t)p->tables,old=(uintptr_t)base->plan.tables,oldbytes=base->plan.capacity*P;
 if(((a|ptr)&(P-1))||!extent(a,PG_TABLE_BYTES)||!extent(ptr,PG_TABLE_BYTES)||ptr+PG_TABLE_BYTES>UINTPTR_MAX||a+PG_TABLE_BYTES>(UINT64_C(1)<<p->pa_bits)||overlaps(a,PG_TABLE_BYTES,base->plan.tables_pa,oldbytes)||overlaps(ptr,PG_TABLE_BYTES,old,oldbytes))return false;
 bool covered=false;
 const struct pg_mapping *fb=&g->mapping;
 if(!same_map(fb,&p->maps[base->plan.count])||g->pat_index>7||g->cache_bits!=cache_flags(g->pat_index)||fb->va!=fb->pa||fb->flags!=PG_WRITE||!extent(fb->pa,fb->bytes)||fb->bytes/P!=g->pages||fb->pa+fb->bytes>(UINT64_C(1)<<p->pa_bits))return false;
 if(fb->va<=p->probe&&p->probe-fb->va<fb->bytes)return false;
 uint64_t pages=g->pages;if(pages>PG_TABLE_PAGES*512u)return false;
 for(size_t i=0;i<base->plan.count;i++){
  const struct pg_mapping *m=&p->maps[i];
  if(!same_map(m,&base->plan.maps[i])||overlaps(m->pa,m->bytes,fb->pa,fb->bytes)||m->bytes/P>PG_TABLE_PAGES*512u-pages)return false;
  pages+=m->bytes/P;
  if(i<base->plan.count-2&&m->flags==PG_WRITE&&a>=m->pa&&a-m->pa<=m->bytes&&PG_TABLE_BYTES<=m->bytes-(a-m->pa))covered=true;
 }
 if(!covered)return false;
 if(!guard)return true;
 if(!e||!e->map||e->live.image.start>=e->live.image.end)return false;
 const struct boot_range forbidden[]={e->live.stack,e->live.handoff,e->live.source,e->workspace};
 for(unsigned i=0;i<3;i++){
  uint64_t x=guard->pa[i],va=S846_IST+3*i*P;bool rw=false;
  if((x&(P-1))||!extent(x,P)||x<e->live.image.start||x+P>e->live.image.end||overlaps(x,P,a,PG_TABLE_BYTES)||overlaps(x,P,base->plan.tables_pa,oldbytes)||overlaps(x,P,fb->pa,fb->bytes))return false;
  for(unsigned j=0;j<i;j++)if(x==guard->pa[j])return false;
  for(unsigned j=0;j<sizeof forbidden/sizeof forbidden[0];j++)if(forbidden[j].start>=forbidden[j].end||overlaps(x,P,forbidden[j].start,forbidden[j].end-forbidden[j].start))return false;
  for(size_t j=0;j<e->map->nr_candidates;j++){struct boot_range q=e->map->candidates[j];if(q.start>=q.end||overlaps(x,P,q.start,q.end-q.start))return false;}
  for(size_t j=0;j<=base->plan.count;j++){
   const struct pg_mapping *m=&p->maps[j];
   if(j<base->plan.count-2&&m->va==m->pa&&m->flags==PG_WRITE&&x>=m->pa&&x-m->pa<m->bytes&&P<=m->bytes-(x-m->pa))rw=true;
   if(overlaps(va-P,3*P,m->va,m->bytes))return false;
  }
  struct pg_mapping want={va,x,P,PG_WRITE};
  if(!rw||!same_map(&want,&p->maps[base->plan.count+1+i])||pages>=PG_TABLE_PAGES*512u)return false;
  pages++;
  if(p->probe>=va-P&&p->probe<va+2*P)return false;
 }
 return true;
}
static bool expected(const struct s842_root *base,const struct s850_desktop_root *r,uint64_t va,uint64_t *out){
 for(size_t i=0;i<r->plan.count;i++){
  const struct pg_mapping *m=&r->plan.maps[i];
  if(va>=m->va&&va-m->va<m->bytes){
   uint64_t pa=m->pa+(va-m->va),bits=(m->flags&PG_WRITE?2u:0u)|(m->flags&PG_EXEC?0:NX);
   if(i==base->plan.count)bits|=r->gop.cache_bits;
   else if(pa==base->lapic||pa==base->ioapic)bits|=cache_flags(base->uc_index);
   *out=pa|1u|bits;return true;
  }
 }
 return false;
}
static bool walk(const struct s842_root *base,const struct s850_desktop_root *r,size_t t,unsigned l,uint64_t prefix,uint32_t *seen,uint64_t *leaves){
 if(t>=r->built.used||(*seen&(UINT32_C(1)<<t)))return false;
 *seen|=UINT32_C(1)<<t;
 const uint64_t mask=((UINT64_C(1)<<r->plan.pa_bits)-1)&~(P-1);
 for(unsigned i=0;i<512;i++){
  uint64_t e=r->plan.tables[t][i],va=prefix|((uint64_t)i<<(12+9*(l-1)));
  if(!e)continue;
  if(l==1){uint64_t w;if(!expected(base,r,va,&w)||(e&~UINT64_C(0x60))!=w)return false;(*leaves)++;}
  else{uint64_t pa=e&mask;if((e&~(mask|UINT64_C(0x20)))!=3u||pa<r->plan.tables_pa||pa-r->plan.tables_pa>=r->built.used*P||!walk(base,r,(size_t)((pa-r->plan.tables_pa)/P),l-1,va,seen,leaves))return false;}
 }
 return true;
}
static bool audit(const struct s842_root *base,const struct s850_gop_plan *g,const struct s846_guard *guard,const struct boot_memory_evidence *e,const struct s850_desktop_root *r){
 if(!r||!r->ready||!same_gop(g,&r->gop)||!shape(base,g,guard,e,&r->plan)||r->built.error!=PG_OK||r->built.root!=r->plan.tables_pa||!r->built.used||r->built.used>r->plan.capacity)return false;
 uint64_t leaves=0,want=0;uint32_t seen=0,all=r->built.used==32?UINT32_MAX:(UINT32_C(1)<<r->built.used)-1;
 for(size_t i=0;i<r->plan.count;i++)want+=r->plan.maps[i].bytes/P;
 if(!walk(base,r,0,4,0,&seen,&leaves)||seen!=all||leaves!=want)return false;
 /* No unexpected data in unused table storage. */
 for(size_t i=r->built.used;i<r->plan.capacity;i++)for(size_t j=0;j<512;j++)if(r->plan.tables[i][j])return false;
 return true;
}
bool s850_desktop_audit_guarded(const struct s842_root *base,const struct s850_gop_request *req,const struct s846_guard *guard,const struct s850_desktop_root *r){
 struct s850_gop_plan g;
 return req&&s842_audit(base,req->cache)&&s850_gop_prepare(req,&g)&&audit(base,&g,guard,req->memory,r);
}
static bool patch(const struct s850_desktop_root *r,uint64_t va,uint64_t bits){
 uint64_t mask=((UINT64_C(1)<<r->plan.pa_bits)-1)&~(P-1);size_t t=0;
 for(unsigned l=4;l>1;l--){uint64_t e=r->plan.tables[t][(va>>(12+9*(l-1)))&511u],pa=e&mask;if((e&~mask)!=3u||pa<r->plan.tables_pa||pa-r->plan.tables_pa>=r->built.used*P)return false;t=(size_t)((pa-r->plan.tables_pa)/P);}
 uint64_t *e=&r->plan.tables[t][(va>>12)&511u];if(*e!=(va|NX|3u))return false;*e|=bits;return true;
}
bool s850_desktop_build_guarded(const struct s842_root *base,const struct s850_gop_request *req,const struct s846_guard *guard,uint64_t (*tables)[512],uint64_t pa,struct s850_desktop_root *out){
 if(!out)return false;
 /* Refuse obvious metadata aliases before writing any output. The trusted
  * caller must also exclude all nested request/boot/reader metadata. */
 uint64_t op=(uintptr_t)out,tp=(uintptr_t)tables,bp=(uintptr_t)base,rp=(uintptr_t)req;
 if(!extent(op,sizeof *out)||(tables&&(!extent(tp,PG_TABLE_BYTES)||overlaps(op,sizeof *out,tp,PG_TABLE_BYTES)))||(base&&(!extent(bp,sizeof *base)||overlaps(op,sizeof *out,bp,sizeof *base)))||(req&&(!extent(rp,sizeof *req)||overlaps(op,sizeof *out,rp,sizeof *req))))return false;
 if(base&&base->plan.tables&&base->plan.capacity<=PG_TABLE_PAGES&&overlaps(op,sizeof *out,(uintptr_t)base->plan.tables,base->plan.capacity*P))return false;
 if(guard&&(!extent((uintptr_t)guard,sizeof *guard)||overlaps(op,sizeof *out,(uintptr_t)guard,sizeof *guard)||(tables&&overlaps(tp,PG_TABLE_BYTES,(uintptr_t)guard,sizeof *guard))))return false;
 *out=(struct s850_desktop_root){0};
 if(!req||!s842_audit(base,req->cache)||base->plan.count>PG_MAX_MAPS-1-(guard?3:0)||!tables||overlaps(tp,PG_TABLE_BYTES,bp,sizeof *base)||overlaps(tp,PG_TABLE_BYTES,rp,sizeof *req))return false;
 struct s850_desktop_root r={.plan=base->plan};
 if(!s850_gop_prepare(req,&r.gop))return false;
 r.plan.tables=tables;r.plan.tables_pa=pa;r.plan.capacity=PG_TABLE_PAGES;r.plan.maps[r.plan.count++]=r.gop.mapping;
 if(guard)for(unsigned i=0;i<3;i++)r.plan.maps[r.plan.count++]=(struct pg_mapping){S846_IST+3*i*P,guard->pa[i],P,PG_WRITE};
 if(!shape(base,&r.gop,guard,req->memory,&r.plan))return false;
 r.built=pg_build(&r.plan);if(r.built.error!=PG_OK||!patch(&r,base->lapic,cache_flags(base->uc_index))||!patch(&r,base->ioapic,cache_flags(base->uc_index)))return false;
 for(uint64_t off=0;off<r.gop.mapping.bytes;off+=P)if(!patch(&r,r.gop.mapping.va+off,r.gop.cache_bits))return false;
 r.ready=true;if(!audit(base,&r.gop,guard,req->memory,&r)||!s842_audit(base,req->cache))return false;
 *out=r;return true;
}

bool s850_desktop_build(const struct s842_root *b,const struct s850_gop_request *r,uint64_t (*t)[512],uint64_t pa,struct s850_desktop_root *o){return s850_desktop_build_guarded(b,r,0,t,pa,o);}
bool s850_desktop_audit(const struct s842_root *b,const struct s850_gop_request *r,const struct s850_desktop_root *o){return s850_desktop_audit_guarded(b,r,0,o);}

#if (defined(STYLEOS_TEST_RUNTIME_ROOTS_861) || defined(STYLEOS_OWNED_SHELL_865))
bool s850_desktop_audit_frozen_861(const struct s842_root *base,const struct s842_cache *cache,const struct s850_gop_plan *proof,const struct boot_memory_evidence *e,const struct s846_guard *guard,const struct s850_desktop_root *r){
 return base&&cache&&proof&&e&&guard&&r&&s842_audit(base,cache)&&audit(base,proof,guard,e,r);
}
#endif
