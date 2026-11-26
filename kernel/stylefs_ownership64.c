#include <styleos/stylefs_ownership64.h>
#include "stylefs_ownership_internal.h"
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static uint64_t get(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
struct collect {struct ow64_inventory *inv;bool full;};
static bool visit(void *ctx,const struct sfs64_map *m){
 struct collect *a=ctx;struct ow64_inventory *v=a->inv;
 if(m->count>v->slots-v->count){a->full=true;return false;}
 uint64_t logical=0;
 for(unsigned i=0;i<m->count;i++){
  const uint8_t *p=m->records+(size_t)i*SFS64_MAP_RECORD;
  struct sfs64_run run={get(p),get(p+8)};
  v->entries[v->count++]=(struct ow64_extent){run,m->file.id,m->file.revision,m->file.bytes,logical,m->file.checksum};
  logical+=run.sectors;
 }return true;
}
static void swap(struct ow64_extent *a,struct ow64_extent *b){struct ow64_extent x=*a;*a=*b;*b=x;}
static void sift(struct ow64_extent *a,size_t n,size_t i){
 while(i<n/2){
  size_t j=i*2+1;if(j+1<n&&a[j+1].run.first>a[j].run.first)++j;
  if(a[i].run.first>=a[j].run.first)return;
  swap(a+i,a+j);i=j;
 }
}
static void sort(struct ow64_extent *a,size_t n){
 for(size_t i=n/2;i;i--)sift(a,n,i-1);
 for(size_t i=n;i>1;i--){swap(a,a+i-1);sift(a,i-1,0);}
}
static bool identical(const struct ow64_extent *a,const struct ow64_extent *b){
 return a->run.first==b->run.first&&a->run.sectors==b->run.sectors&&a->id==b->id&&
 a->revision==b->revision&&a->bytes==b->bytes&&a->logical==b->logical&&a->checksum==b->checksum;
}
enum ow64_status ow64_collect(struct ic64_state *s,struct ic64_workspace *w,
 struct ow64_inventory inv[2],struct ow64_result *out){
 if(!s||!s->table.valid)return OW64_STALE;
 if(!inv||!out||!w)return OW64_INVALID;
 for(unsigned i=0;i<2;i++)if(!inv[i].slots||inv[i].slots>SIZE_MAX/sizeof *inv[i].entries||
    inv[i].slots>SIZE_MAX/sizeof *inv[i].runs)return OW64_INVALID;
 const void *ptr[]={s,w,inv,out,s->table.block,inv[0].entries,inv[0].runs,inv[1].entries,inv[1].runs};
 const size_t sizes[]={sizeof *s,sizeof *w,2*sizeof *inv,sizeof *out,sizeof *s->table.block,
 inv[0].slots*sizeof *inv[0].entries,inv[0].slots*sizeof *inv[0].runs,
 inv[1].slots*sizeof *inv[1].entries,inv[1].slots*sizeof *inv[1].runs};
 for(unsigned i=0;i<9;i++)for(unsigned k=0;k<i;k++)if(!sep(ptr[i],sizes[i],ptr[k],sizes[k]))return OW64_INVALID;
 *out=(struct ow64_result){0};inv[0].count=inv[1].count=0;
 struct it64_state tables[2];enum ic64_status rc=ic64_strict_banks(s,w,tables);
 if(rc!=IC64_OK)return rc==IC64_IO?OW64_IO:rc==IC64_STALE?OW64_STALE:OW64_CORRUPT;
 for(unsigned i=0;i<2;i++){
  if(!tables[i].valid)continue;
  struct collect a={inv+i,false};
  enum it64_status tr=it64_walk(tables+i,w->page,visit,&a);
  if(tr!=IT64_OK)return a.full?OW64_OUTPUT:tr==IT64_IO?OW64_IO:OW64_CORRUPT;
  sort(inv[i].entries,inv[i].count);
  uint64_t end=tables[i].geometry.data_start;
  for(size_t j=0;j<inv[i].count;j++){
   struct sfs64_run run=inv[i].entries[j].run;
   if(run.first<end)return OW64_CORRUPT;
   end=run.first+run.sectors;inv[i].runs[j]=run;
  }
 }
 /* Merge both disjoint sorted lists. Sharing only exact immutable extents. */
 size_t i=0,j=0;
 while(i<inv[0].count&&j<inv[1].count){
  const struct ow64_extent *a=&inv[0].entries[i],*b=&inv[1].entries[j];
  if(a->run.first+a->run.sectors<=b->run.first){++i;continue;}
  if(b->run.first+b->run.sectors<=a->run.first){++j;continue;}
  if(!identical(a,b))return OW64_CORRUPT;
  ++i;++j;
 }
 struct sfs64_inventory a={inv[0].runs,inv[0].count},b={inv[1].runs,inv[1].count};
 struct sfs64_space space;uint64_t cap=s->table.geometry.capacity,start=s->table.geometry.data_start;
 if(sfs64_space_query(cap,start,a,b,&space)!=SFS64_ALLOC_OK)return OW64_CORRUPT;
 *out=(struct ow64_result){s->table.geometry.epoch,cap,start,space,true};return OW64_OK;
}
