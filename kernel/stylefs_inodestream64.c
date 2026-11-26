#include <styleos/stylefs_inodestream64.h>
#include "stylefs_ownership_internal.h"
#include "stylefs_stream_crc_internal.h"
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static const struct bs_grant *grant(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t length){
 if(!b||!token||!owner||b->busy)return 0;
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=b->grants+i;
  if(g->token!=token)continue;
  if(g->owner!=owner||g->length!=length||g->rights!=(BS_READ|BS_WRITE)||g->device>=BS_DEVICES)return 0;
  const struct bs_device *d=b->devices+g->device;
  if(!g->generation||g->generation!=d->generation||d->recovery||!d->backend.writable||d->backend.system_media||
     g->start>=d->backend.sectors||g->length>d->backend.sectors-g->start)return 0;
  return g;
 }return 0;
}
static bool paired(struct ic64_state *s,uint64_t token){
 struct it64_state *t=&s->table;
 const struct bs_grant *a=grant(t->block,t->owner,t->token,s->sectors),
 *b=grant(t->block,t->owner,token,t->geometry.capacity);
 return a&&b&&a->device==b->device&&a->generation==b->generation&&
 (a->start+a->length<=b->start||b->start+b->length<=a->start);
}
static bool max_id(void *ctx,const struct sfs64_map *m){
 uint64_t *id=ctx;if(m->file.id>*id)*id=m->file.id;return true;
}
struct merge {struct it64_state old;struct is64_workspace *w;uint64_t page,ordinal;unsigned at,count;size_t bytes;enum it64_status error;};
static bool merged(void *ctx,uint64_t i,uint8_t *out,size_t limit,size_t *n){
 struct merge *m=ctx;
 if(i!=m->ordinal++)return false;
 struct it64_blob blob;
 if(i==m->old.count)blob=(struct it64_blob){m->w->manifest,m->bytes};
 else{
  if(m->at==m->count){
   m->error=it64_page_blobs(&m->old,m->page++,m->w->old_page,m->w->old,&m->count);
   if(m->error!=IT64_OK)return false;
   m->at=0;
  }
  blob=m->w->old[m->at++];
 }
 if(blob.length>limit)return false;
 for(size_t j=0;j<blob.length;j++)out[j]=blob.bytes[j];
 *n=blob.length;return true;
}
enum is64_status is64_create(struct ic64_state *s,uint64_t token,struct is64_workspace *w,
 struct ow64_inventory inv[2],uint64_t id,uint64_t bytes,is64_source source,void *ctx){
 if(!s||!s->table.valid)return IS64_STALE;
 if(!id||(bytes&&!source)||!inv||!w||!paired(s,token))return IS64_INVALID;
 for(unsigned i=0;i<2;i++)if(!inv[i].slots||inv[i].slots>SIZE_MAX/sizeof *inv[i].entries||
    inv[i].slots>SIZE_MAX/sizeof *inv[i].runs)return IS64_INVALID;
 const void *ptr[]={s,w,inv,s->table.block,inv[0].entries,inv[0].runs,inv[1].entries,inv[1].runs};
 const size_t sizes[]={sizeof *s,sizeof *w,2*sizeof *inv,sizeof *s->table.block,
 inv[0].slots*sizeof *inv[0].entries,inv[0].slots*sizeof *inv[0].runs,
 inv[1].slots*sizeof *inv[1].entries,inv[1].slots*sizeof *inv[1].runs};
 for(unsigned i=0;i<8;i++)for(unsigned j=0;j<i;j++)if(!sep(ptr[i],sizes[i],ptr[j],sizes[j]))return IS64_INVALID;
 struct ow64_result proof;
 enum ow64_status ow=ow64_collect(s,&w->commit,inv,&proof);
 if(ow!=OW64_OK)return ow==OW64_OUTPUT?IS64_FULL:ow==OW64_STALE?IS64_STALE:ow==OW64_IO?IS64_IO:IS64_CORRUPT;
 struct it64_state tables[2];
 enum ic64_status ic=ic64_strict_banks(s,&w->commit,tables);
 if(ic!=IC64_OK)return ic==IC64_IO?IS64_IO:IS64_CORRUPT;
 uint64_t max=0;
 for(unsigned i=0;i<2;i++)if(tables[i].valid){
  enum it64_status it=it64_walk(&tables[i],w->old_page,max_id,&max);
  if(it!=IT64_OK)return it==IT64_IO?IS64_IO:IS64_CORRUPT;
 }
 if(id<=max)return IS64_INVALID;
 if(s->table.count==UINT64_MAX||s->table.geometry.epoch==UINT64_MAX)return IS64_FULL;
 struct sfs64_inventory a={inv[0].runs,inv[0].count},b={inv[1].runs,inv[1].count};
 size_t nr=0;
 enum sfs64_alloc_status ar=sfs64_allocate(proof.capacity,proof.data_start,a,b,bytes,w->plan,IS64_RUNS,&nr);
 if(ar!=SFS64_ALLOC_OK)return ar==SFS64_ALLOC_INVALID?IS64_CORRUPT:IS64_FULL;
 /* Existing bounded, flushed staging primitive; read back canonical sectors. */
 struct it64_state *t=&s->table;
 uint64_t off=0;uint32_t crc=UINT32_MAX;
 for(size_t i=0;i<nr;i++){
  struct sfs64_run run=w->plan[i];uint64_t remain=bytes-off;
  uint64_t extent_bytes=run.sectors>=sfs64_sectors(remain)?remain:run.sectors*512;
  struct sfs_extent64 e={run.first,extent_bytes};
  for(uint64_t local=0;local<extent_bytes;){
   size_t n=extent_bytes-local<sizeof w->buffer?(size_t)(extent_bytes-local):sizeof w->buffer,done=0;
   if(!source(ctx,off,w->buffer,n))return IS64_CANCELLED;
   if(!paired(s,token)||sfs64_stage_window(t->block,t->owner,token,e,local,w->buffer,n,&done)!=BS_OK||done!=n){
    t->valid=false;return IS64_UNCERTAIN;
   }
   for(size_t k=0;k<n;k+=512){
    uint8_t check[512];size_t used=n-k<512?n-k:512;
    if(bs_read_sector(t->block,t->owner,token,e.first+local/512+k/512,check)!=BS_OK){t->valid=false;return IS64_UNCERTAIN;}
    for(size_t j=0;j<512;j++)if(check[j]!=(j<used?w->buffer[k+j]:0)){t->valid=false;return IS64_UNCERTAIN;}
   }
   crc=sfs64_stream_crc_update(crc,w->buffer,n);local+=n;off+=n;
  }
 }
 if(off!=bytes){t->valid=false;return IS64_CORRUPT;}
 size_t n=sfs64_map_length(nr);
 struct sfs64_file file={id,1,bytes,~crc};
 if(!sfs64_map_encode(w->manifest,n,file,proof.capacity,proof.data_start,w->plan,nr))return IS64_CORRUPT;
 struct merge m={.old=*t,.w=w,.bytes=n,.error=IT64_OK};
 ic=ic64_publish(s,&w->commit,t->count+1,merged,&m);
 if(ic==IC64_OK)return IS64_OK;
 if(ic==IC64_FULL)return IS64_FULL;
 if(ic==IC64_IO||ic==IC64_UNCERTAIN||m.error==IT64_IO){t->valid=false;return IS64_UNCERTAIN;}
 return ic==IC64_STALE?IS64_STALE:IS64_CORRUPT;
}
