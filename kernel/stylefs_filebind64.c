#include <styleos/stylefs_filebind64.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static const struct bs_grant *grant(const struct bs_service *b,uint64_t owner,uint64_t token,uint64_t n){
 if(!b||!owner||!token||b->busy)return 0;
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=&b->grants[i];
  if(g->token!=token)continue;
  if(g->owner!=owner||g->length!=n||!(g->rights&BS_READ)||g->device>=BS_DEVICES)return 0;
  const struct bs_device *d=&b->devices[g->device];
  if(!g->generation||d->generation!=g->generation||d->recovery||g->start>=d->backend.sectors||
     n>d->backend.sectors-g->start)return 0;
  return g;
 }return 0;
}
static uint64_t paired(struct dc64_state *d,struct sfs64_journal *j,bool write){
 if(!d||!d->valid||!j||!sfs64_journal_ready(j,write)||d->block!=j->block||d->owner!=j->owner||j->writer)return 0;
 const struct bs_grant *a=grant(d->block,d->owner,d->token,d->sectors),
 *b=grant(j->block,j->owner,j->token,j->capacity);
 if(!a||!b||a->generation!=b->generation||a->device!=b->device||
    (write&&(!(a->rights&BS_WRITE)||!(b->rights&BS_WRITE)))||
    (a->start<b->start+b->length&&b->start<a->start+a->length))return 0;
 return a->generation;
}
static bool proof_ok(struct dc64_state *d,const struct ns64_proof *p){
 return d&&p&&p->valid&&p->source==d&&p->epoch==d->epoch&&p->token==d->token&&
 p->count==d->count&&p->nodes&&p->count<=SIZE_MAX/sizeof *p->nodes;
}
static bool live(struct fb64_view *v,bool write){
 return v&&v->valid&&proof_ok(v->directory,v->proof)&&v->data&&
 v->directory->epoch==v->epoch&&v->directory->token==v->dir_token&&v->data->token==v->data_token&&
 paired(v->directory,v->data,write)==v->device_generation&&v->device_generation&&
 v->data->generation[v->data->active]==v->generation;
}
static uint32_t get32(const uint8_t *p){uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static bool bank_has(struct sfs64_journal *j,unsigned bank,uint64_t id){
 if(!j->generation[bank])return false;
 for(size_t at=0;at<j->length[bank];){
  uint32_t n=get32(j->payload[bank]+at);at+=4;
  struct sfs64_map m;
  if(!sfs64_map_decode(j->payload[bank]+at,n,j->capacity,j->data_start,&m))return true;
  if(m.file.id==id)return true;
  at+=n;
 }return false;
}
static bool snapshots(struct sfs64_journal *j){
 for(unsigned i=0;i<2;i++)if(j->generation[i]&&!sfs64_snapshot_valid(j->payload[i],j->length[i],j->capacity,j->data_start))return false;
 return j->generation[j->active]!=0;
}
static const struct ns64_node *node(const struct ns64_proof *p,uint64_t id){
 size_t lo=0,hi=(size_t)p->count;
 while(lo<hi){size_t m=lo+(hi-lo)/2;if(p->nodes[m].id<id)lo=m+1;else hi=m;}
 return lo<p->count&&p->nodes[lo].id==id?p->nodes+lo:0;
}
static bool external(struct fb64_view *v,const void *p,size_t n){
 return sep(p,n,v,sizeof *v)&&sep(p,n,v->directory,sizeof *v->directory)&&
 sep(p,n,v->proof,sizeof *v->proof)&&sep(p,n,v->proof->nodes,(size_t)v->proof->count*sizeof *v->proof->nodes)&&
 sep(p,n,v->data,sizeof *v->data)&&sep(p,n,v->data->block,sizeof *v->data->block);
}
enum fb64_status fb64_bind(struct fb64_view *v,struct dc64_state *d,struct ns64_proof *p,struct sfs64_journal *j){
 if(!v||!d||!p||!j||!d->block||!j->block||
    !sep(v,sizeof *v,d,sizeof *d)||!sep(v,sizeof *v,p,sizeof *p)||!sep(v,sizeof *v,j,sizeof *j)||
    !sep(v,sizeof *v,d->block,sizeof *d->block)||!sep(v,sizeof *v,j->block,sizeof *j->block)||
    !proof_ok(d,p)||!sep(v,sizeof *v,p->nodes,(size_t)p->count*sizeof *p->nodes))return FB64_INVALID;
 *v=(struct fb64_view){0};uint64_t dev=paired(d,j,false);
 if(!dev)return FB64_STALE;
 if(!snapshots(j))return FB64_CORRUPT;
 for(size_t i=0;i<p->count;i++){
  const struct ns64_node *x=&p->nodes[i];struct sfs64_file file;
  bool present=sfs64_stream_stat(j,x->id,&file)==SJ_OK;
  if((x->kind==SD64_FILE)!=present)return FB64_CORRUPT;
 }
 if(bank_has(j,j->active,NS64_ROOT))return FB64_CORRUPT;
 *v=(struct fb64_view){d,p,j,d->epoch,j->generation[j->active],d->token,j->token,dev,true};
 return FB64_OK;
}
enum fb64_status fb64_stat(struct fb64_view *v,const uint8_t *path,size_t n,
 uint8_t scratch[SD64_PAGE],struct sfs64_file *out){
 if(!live(v,false))return FB64_STALE;
 if(!external(v,scratch,SD64_PAGE)||!external(v,out,sizeof *out)||!sep(out,sizeof *out,scratch,SD64_PAGE)||
    !sep(out,sizeof *out,path,n))return FB64_INVALID;
 uint64_t id;uint8_t kind;
 enum ns64_status rc=ns64_resolve(v->directory,v->proof,path,n,scratch,&id,&kind);
 if(rc!=NS64_OK)return rc==NS64_IO?FB64_IO:rc==NS64_CORRUPT?FB64_CORRUPT:rc==NS64_STALE?FB64_STALE:FB64_INVALID;
 if(kind!=SD64_FILE)return FB64_INVALID;
 struct sfs64_file file;
 if(sfs64_stream_stat(v->data,id,&file)!=SJ_OK){v->valid=false;return FB64_CORRUPT;}
 *out=file;return FB64_OK;
}
enum fb64_status fb64_create(struct fb64_view *v,struct fb64_workspace *w,
 const struct sd64_record *e,uint64_t bytes,fb64_source source,void *ctx){
 if(!live(v,true))return FB64_STALE;
 if(!external(v,w,sizeof *w)||!external(v,e,sizeof *e)||!sep(w,sizeof *w,e,sizeof *e)||
    !e||e->kind!=SD64_FILE||e->target<=NS64_ROOT||!sd64_key_valid(&e->key)||(bytes&&!source))return FB64_INVALID;
 struct dc64_state *d=v->directory;struct sfs64_journal *j=v->data;
 const struct ns64_node *parent=node(v->proof,e->key.parent);
 if(e->key.parent!=NS64_ROOT&&(!parent||parent->kind!=SD64_DIRECTORY))return FB64_INVALID;
 if(!snapshots(j))return FB64_CORRUPT;
 if(node(v->proof,e->target)||bank_has(j,0,e->target)||bank_has(j,1,e->target))return FB64_CONFLICT;
 if(d->count){
  uint64_t id;uint8_t kind;
  enum sd64_status rc=sd64_lookup(d->block,d->owner,d->token,&d->root,&e->key,w->reader.page,&id,&kind);
  if(rc==SD64_OK)return FB64_CONFLICT;
  if(rc!=SD64_MISSING){v->valid=false;return rc==SD64_IO?FB64_IO:FB64_CORRUPT;}
 }
 w->stream=(struct sfs64_stream){0};
 enum sj_status sr=sfs64_stream_begin(&w->stream,j,e->target,0,bytes);
 if(sr!=SJ_OK)return sr==SJ_FULL?FB64_FULL:sr==SJ_CORRUPT?FB64_CORRUPT:FB64_STALE;
 for(uint64_t off=0;off<bytes;){
  size_t n=bytes-off<sizeof w->buffer?(size_t)(bytes-off):sizeof w->buffer;
  if(!source(ctx,off,w->buffer,n)){sfs64_stream_cancel(&w->stream);return FB64_CANCELLED;}
  sr=sfs64_stream_append(&w->stream,w->buffer,n);
  if(sr!=SJ_OK){sfs64_stream_cancel(&w->stream);v->valid=false;return FB64_UNCERTAIN;}
  off+=n;
 }
 sr=sfs64_stream_finish(&w->stream);
 if(sr!=SJ_OK){v->valid=false;return FB64_UNCERTAIN;}
 /* Durable file manifest is now reachable by ID, but NOT by name.
  * Do not reclaim it if directory publication refuses or is interrupted. */
 struct dc64_edit edit={.op=DC64_INSERT,.entry=*e};
 enum ns64_status nr=ns64_apply(d,v->proof,&w->writer,&w->reader,&edit);
 v->valid=false;
 if(nr==NS64_OK)return FB64_OK;
 if(nr==NS64_UNCERTAIN||nr==NS64_IO||!d->valid)return FB64_UNCERTAIN;
 return FB64_ORPHAN;
}
