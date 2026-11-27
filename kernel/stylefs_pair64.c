
#include <styleos/stylefs_pair64.h>
static const uint8_t magic[8]={'S','F','6','4','P','A','I','1'};
struct spans {uint64_t receipt,dir,data;};
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool equal(const uint8_t *p,const uint8_t *q,size_t n){for(size_t i=0;i<n;i++)if(p[i]!=q[i])return false;return true;}
static bool zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t c=UINT32_MAX;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}return ~c;}
static const struct bs_grant *grant(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t n,bool write){
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=&b->grants[i];
  if(g->token!=token)continue;
  if(g->owner!=owner||g->length!=n||g->device>=BS_DEVICES||
     !(g->rights&BS_READ)||(write&&!(g->rights&BS_WRITE)))return 0;
  const struct bs_device *d=&b->devices[g->device];
  if(!g->generation||g->generation!=d->generation||d->recovery||
     g->start>=d->backend.sectors||n>d->backend.sectors-g->start||
     (write&&(!d->backend.writable||d->backend.system_media)))return 0;
  return g;
 }return 0;
}
static bool overlap(const struct bs_grant *a,const struct bs_grant *b){return a->start<b->start+b->length&&b->start<a->start+a->length;}
static bool layout(struct bs_service *b,const struct sp64_layout *l,bool write,struct spans *out){
 if(!b||b->busy||!l||!l->owner||!l->receipt||!l->dir||!l->data||
    l->dir_sectors<18||(l->dir_sectors-2)%16||l->data_start<SFS64_JOURNAL_SECTORS||l->data_start>=l->data_sectors)return false;
 const struct bs_grant *a=grant(b,l->owner,l->receipt,2,write),*d=grant(b,l->owner,l->dir,l->dir_sectors,false),
 *f=grant(b,l->owner,l->data,l->data_sectors,false);
 if(!a||!d||!f||a->device!=d->device||a->device!=f->device||
    a->generation!=d->generation||a->generation!=f->generation||
    overlap(a,d)||overlap(a,f)||overlap(d,f))return false;
 *out=(struct spans){a->start,d->start,f->start};return true;
}
static bool args(struct bs_service *b,const struct sp64_layout *l,struct sp64_workspace *w,struct dc64_node *a,size_t cap,struct sp64_pair *out){
 if(!cap||cap>SIZE_MAX/sizeof *a)return false;
 const void *p[]={b,l,w,a,out};size_t n[]={sizeof *b,sizeof *l,sizeof *w,cap*sizeof *a,sizeof *out};
 for(unsigned i=0;i<5;i++)for(unsigned j=0;j<i;j++)if(!sep(p[i],n[i],p[j],n[j]))return false;
 return true;
}
static void encode(uint8_t p[512],const struct sp64_layout *l,const struct spans *s,const struct sp64_pair *v){
 for(unsigned i=0;i<512;i++)p[i]=0;
 for(unsigned i=0;i<8;i++)p[i]=magic[i];
 put(p+8,v->sequence,8);put(p+16,v->dir_epoch,8);put(p+24,v->data_epoch,8);
 put(p+32,s->receipt,8);put(p+40,s->dir,8);put(p+48,s->data,8);
 put(p+56,l->dir_sectors,8);put(p+64,l->data_sectors,8);put(p+72,l->data_start,8);
 put(p+80,v->slot,4);put(p+84,crc(p,84),4);
}
static bool decode(const uint8_t p[512],const struct sp64_layout *l,const struct spans *s,unsigned slot,struct sp64_pair *v){
 if(!equal(p,magic,8)||!get(p+8,8)||!get(p+16,8)||!get(p+24,8)||
 get(p+32,8)!=s->receipt||get(p+40,8)!=s->dir||get(p+48,8)!=s->data||
 get(p+56,8)!=l->dir_sectors||get(p+64,8)!=l->data_sectors||get(p+72,8)!=l->data_start||
 get(p+80,4)!=slot||get(p+84,4)!=crc(p,84)||!zero(p+88,424))return false;
 *v=(struct sp64_pair){get(p+8,8),get(p+16,8),get(p+24,8),slot};return true;
}
static enum sp64_status select_pair(struct bs_service *b,const struct sp64_layout *l,struct sp64_workspace *w,
 struct dc64_node *a,size_t cap,const struct sp64_pair *v){
 enum dc64_status dr=dc64_mount_epoch(&w->dir,b,l->owner,l->dir,l->dir_sectors,&w->dw.publish,v->dir_epoch);
 if(dr!=DC64_OK)return dr==DC64_IO?SP64_IO:dr==DC64_INVALID?SP64_INVALID:SP64_CORRUPT;
 dr=dc64_hierarchy_selected(&w->dir,&w->dw,a,cap);
 if(dr!=DC64_OK)return dr==DC64_FULL?SP64_FULL:dr==DC64_IO?SP64_IO:dr==DC64_INVALID?SP64_INVALID:SP64_CORRUPT;
 enum sj_status jr=sfs64_journal_mount(&w->data,b,l->owner,l->data,l->data_sectors,l->data_start);
 if(jr!=SJ_OK)return jr==SJ_IO?SP64_IO:jr==SJ_STALE?SP64_INVALID:SP64_CORRUPT;
 if(w->data.generation[0]==v->data_epoch)w->data.active=0;
 else if(w->data.generation[1]==v->data_epoch)w->data.active=1;
 else return SP64_CORRUPT;
 /* Unique graph IDs already checked. Each file must have exactly one
  * manifest, and the count must equal the complete manifest inventory.
  * Directory IDs cannot alias a file manifest. */
 size_t files=0;
 for(size_t i=0;i<(size_t)w->dir.count;i++){
  if(a[i].kind!=SD64_FILE)continue;
  struct sfs64_file f;
  if(sfs64_stream_stat(&w->data,a[i].id,&f)!=SJ_OK)return SP64_CORRUPT;
  files++;
 }
 size_t n=w->data.length[w->data.active],at=0,maps=0;
 const uint8_t *p=w->data.payload[w->data.active];
 while(at<n){
  if(n-at<4)return SP64_CORRUPT;
  uint64_t len=get(p+at,4);at+=4;
  if(!len||len>n-at)return SP64_CORRUPT;
  at+=(size_t)len;maps++;
 }
 return files==maps?SP64_OK:SP64_CORRUPT;
}
enum sp64_status sp64_mount(struct bs_service *b,const struct sp64_layout *l,struct sp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sp64_pair *out){
 struct spans s;
 if(!args(b,l,w,a,cap,out)||!layout(b,l,false,&s))return SP64_INVALID;
 struct sp64_pair v[2];bool good[2];
 for(unsigned i=0;i<2;i++){
  uint8_t p[512];
  if(bs_read_sector(b,l->owner,l->receipt,i,p)!=BS_OK)return SP64_IO;
  good[i]=decode(p,l,&s,i,&v[i]);
 }
 if(good[0]&&good[1]&&v[0].sequence==v[1].sequence)return SP64_CORRUPT;
 unsigned first=good[1]&&(!good[0]||v[1].sequence>v[0].sequence)?1:0;
 for(unsigned i=0;i<2;i++){
  unsigned slot=first^i;if(!good[slot])continue;
  enum sp64_status rc=select_pair(b,l,w,a,cap,&v[slot]);
  if(rc==SP64_OK){*out=v[slot];return rc;}
  if(rc!=SP64_CORRUPT)return rc;
 }
 return SP64_CORRUPT;
}
static enum sp64_status publish(struct bs_service *b,const struct sp64_layout *l,const struct spans *s,
 const struct sp64_pair *v){
 uint8_t p[512],check[512];encode(p,l,s,v);
 if(bs_write_sector(b,l->owner,l->receipt,v->slot,p)!=BS_OK||
    bs_read_sector(b,l->owner,l->receipt,v->slot,check)!=BS_OK||!equal(p,check,512))return SP64_UNCERTAIN;
 return SP64_OK;
}
enum sp64_status sp64_commit(struct bs_service *b,const struct sp64_layout *l,struct sp64_workspace *w,
 struct dc64_node *a,size_t cap,uint64_t expected,uint64_t de,uint64_t fe,struct sp64_pair *out){
 struct spans s;struct sp64_pair old;
 if(!args(b,l,w,a,cap,out)||!layout(b,l,true,&s)||!expected)return SP64_INVALID;
 enum sp64_status rc=sp64_mount(b,l,w,a,cap,&old);
 if(rc!=SP64_OK)return rc;
 if(old.sequence!=expected||de<old.dir_epoch||fe<old.data_epoch||
    de-old.dir_epoch>1||fe-old.data_epoch>1||(de==old.dir_epoch&&fe==old.data_epoch))return SP64_INVALID;
 if(old.sequence==UINT64_MAX)return SP64_FULL;
 struct sp64_pair v={old.sequence+1,de,fe,old.slot^1u};
 rc=select_pair(b,l,w,a,cap,&v);
 if(rc!=SP64_OK)return rc;
 rc=publish(b,l,&s,&v);
 if(rc==SP64_OK)*out=v;
 return rc;
}
#ifdef STYLEFS_HOST_FORMAT
enum sp64_status sp64_provision(struct bs_service *b,const struct sp64_layout *l,struct sp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sp64_pair *out){
 struct spans s;if(!args(b,l,w,a,cap,out)||!layout(b,l,true,&s))return SP64_INVALID;
 for(unsigned i=0;i<2;i++){
  uint8_t p[512];if(bs_read_sector(b,l->owner,l->receipt,i,p)!=BS_OK)return SP64_IO;
  if(!zero(p,512))return SP64_INVALID;
 }
 struct sp64_pair v={1,1,1,0};
 enum sp64_status rc=select_pair(b,l,w,a,cap,&v);
 if(rc!=SP64_OK)return rc;
 if(w->dir.count||w->data.length[w->data.active])return SP64_INVALID;
 rc=publish(b,l,&s,&v);if(rc==SP64_OK)*out=v;return rc;
}
#endif

/* All receipt invalidations are complete sector writes + flush + readback.
 * Once an I/O failure occurs the block service quarantines the device. */
static enum sp64_status clear_receipt(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t at){
 uint8_t blank[512]={0},check[512];
 if(bs_write_sector(b,owner,token,at,blank)!=BS_OK||
    bs_read_sector(b,owner,token,at,check)!=BS_OK||!zero(check,512))return SP64_UNCERTAIN;
 return SP64_OK;
}
static enum sp64_status from_dir(enum dc64_status rc){
 switch(rc){
 case DC64_OK:return SP64_OK;
 case DC64_FULL:return SP64_FULL;
 case DC64_IO:return SP64_IO;
 case DC64_UNCERTAIN:return SP64_UNCERTAIN;
 case DC64_CORRUPT:return SP64_CORRUPT;
 case DC64_CANCELLED:return SP64_CANCELLED;
 default:return SP64_INVALID;
 }
}
static enum sp64_status from_data(enum sj_status rc){
 switch(rc){
 case SJ_OK:return SP64_OK;
 case SJ_FULL:return SP64_FULL;
 case SJ_IO:return SP64_IO;
 case SJ_UNCERTAIN:return SP64_UNCERTAIN;
 case SJ_CORRUPT:case SJ_SIGNATURE:return SP64_CORRUPT;
 default:return SP64_INVALID;
 }
}
enum sp64_status sp64_prepare(struct bs_service *b,const struct sp64_layout *l,struct sp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sp64_pair *out){
 struct spans s;
 if(!args(b,l,w,a,cap,out)||!layout(b,l,true,&s)||
    !grant(b,l->owner,l->dir,l->dir_sectors,true)||!grant(b,l->owner,l->data,l->data_sectors,true))return SP64_INVALID;
 struct sp64_pair selected,other;
 enum sp64_status rc=sp64_mount(b,l,w,a,cap,&selected);
 if(rc!=SP64_OK)return rc;
 /* Read all evidence before the first recovery write. */
 uint8_t p[512];unsigned other_slot=selected.slot^1u;
 if(bs_read_sector(b,l->owner,l->receipt,other_slot,p)!=BS_OK)return SP64_IO;
 bool clear_pair=false;
 if(decode(p,l,&s,other_slot,&other)){
  if(other.sequence>selected.sequence){
   if(other.sequence-selected.sequence!=1||other.dir_epoch<selected.dir_epoch||
      other.data_epoch<selected.data_epoch||other.dir_epoch-selected.dir_epoch>1||
      other.data_epoch-selected.data_epoch>1)return SP64_CORRUPT;
   clear_pair=true;
  }else{
   if(other.sequence==selected.sequence||selected.sequence-other.sequence!=1||
      other.dir_epoch>selected.dir_epoch||other.data_epoch>selected.data_epoch||
      selected.dir_epoch-other.dir_epoch>1||selected.data_epoch-other.data_epoch>1)return SP64_CORRUPT;
  }
 }else if(!zero(p,512))clear_pair=true;
 struct dc64_state latest;
 enum dc64_status dr=dc64_mount(&latest,b,l->owner,l->dir,l->dir_sectors,&w->dw.publish);
 if(dr!=DC64_OK)return from_dir(dr);
 if(latest.epoch<selected.dir_epoch||latest.epoch-selected.dir_epoch>1||
    (latest.epoch>selected.dir_epoch&&latest.active==w->dir.active))return SP64_CORRUPT;
 bool clear_dir=latest.epoch>selected.dir_epoch;
 unsigned data_bank=w->data.active^1u;uint64_t data_epoch=w->data.generation[data_bank];
 if(data_epoch>selected.data_epoch&&data_epoch-selected.data_epoch>1)return SP64_CORRUPT;
 bool clear_data=data_epoch>selected.data_epoch;
 /* First retire any dangling global receipt so it cannot become valid again
  * if an abandoned epoch number is reused. The selected receipt remains. */
 if(clear_pair&&(rc=clear_receipt(b,l->owner,l->receipt,other_slot))!=SP64_OK)return rc;
 if(clear_dir&&(rc=clear_receipt(b,l->owner,l->dir,latest.active))!=SP64_OK)return rc;
 if(clear_data&&(rc=clear_receipt(b,l->owner,l->data,1+(uint64_t)data_bank*(1+SFS64_BANK_SECTORS)))!=SP64_OK)return rc;
 /* Re-open and independently check both raw latest versions match selected. */
 struct sp64_pair check;
 rc=sp64_mount(b,l,w,a,cap,&check);
 if(rc!=SP64_OK)return rc;
 if(check.sequence!=selected.sequence||check.slot!=selected.slot||check.dir_epoch!=selected.dir_epoch||
    check.data_epoch!=selected.data_epoch)return SP64_CORRUPT;
 dr=dc64_mount(&latest,b,l->owner,l->dir,l->dir_sectors,&w->dw.publish);
 if(dr!=DC64_OK)return from_dir(dr);
 if(latest.epoch!=check.dir_epoch||latest.active!=w->dir.active||
    w->data.generation[w->data.active^1u]>check.data_epoch)return SP64_CORRUPT;
 *out=check;return SP64_OK;
}
enum sp64_status sp64_put_file(struct bs_service *b,const struct sp64_layout *l,struct sp64_put_workspace *w,
 struct dc64_node *a,size_t cap,const struct sd64_key *key,uint64_t id,uint64_t revision,uint64_t bytes,
 sp64_source read,void *ctx,struct sp64_pair *out){
 if(!w||!cap||cap>SIZE_MAX/sizeof *a||!sd64_key_valid(key)||id<=1||
    revision==UINT64_MAX||(bytes&&!read)||
    !sep(w,sizeof *w,b,sizeof *b)||!sep(w,sizeof *w,l,sizeof *l)||
    !sep(w,sizeof *w,a,cap*sizeof *a)||!sep(w,sizeof *w,out,sizeof *out)||
    !sep(key,sizeof *key,w,sizeof *w)||!sep(key,sizeof *key,a,cap*sizeof *a)||
    !sep(key,sizeof *key,b,sizeof *b)||!sep(key,sizeof *key,l,sizeof *l)||
    !sep(key,sizeof *key,out,sizeof *out)||w->stream.active)return SP64_INVALID;
 struct sp64_pair current;
 enum sp64_status rc=sp64_prepare(b,l,&w->pair,a,cap,&current);
 if(rc!=SP64_OK)return rc;
 if(current.sequence==UINT64_MAX||current.data_epoch==UINT64_MAX||
    (!revision&&current.dir_epoch==UINT64_MAX))return SP64_FULL;
 struct dc64_state *d=&w->pair.dir;struct sfs64_journal *j=&w->pair.data;
 bool parent=key->parent==1,used=false;
 for(size_t i=0;i<(size_t)d->count;i++){
  if(a[i].id==id)used=true;
  if(a[i].id==key->parent&&a[i].kind==SD64_DIRECTORY)parent=true;
 }
 if(!parent||used!= (revision!=0))return SP64_INVALID;
 uint64_t found=0;uint8_t kind=0;
 enum sd64_status lookup=d->count?sd64_lookup(b,l->owner,l->dir,&d->root,key,w->pair.dw.page,&found,&kind):SD64_MISSING;
 if(lookup!=SD64_MISSING&&lookup!=SD64_OK)return lookup==SD64_IO?SP64_IO:SP64_CORRUPT;
 if(revision){
  if(lookup!=SD64_OK||found!=id||kind!=SD64_FILE)return SP64_INVALID;
  struct sfs64_file f;if(sfs64_stream_stat(j,id,&f)!=SJ_OK||f.revision!=revision)return SP64_INVALID;
 }else{
  if(lookup!=SD64_MISSING)return SP64_INVALID;
  if(d->count>=cap)return SP64_FULL;
 }
 enum sj_status jr=sfs64_stream_begin(&w->stream,j,id,revision,bytes);
 if(jr!=SJ_OK)return from_data(jr);
 for(uint64_t at=0;at<bytes;){
  uint64_t left=bytes-at;size_t n=left>sizeof w->transfer?sizeof w->transfer:(size_t)left;
  if(!read(ctx,at,w->transfer,n)){sfs64_stream_cancel(&w->stream);return SP64_CANCELLED;}
  jr=sfs64_stream_append(&w->stream,w->transfer,n);
  if(jr!=SJ_OK){sfs64_stream_cancel(&w->stream);return from_data(jr);}
  at+=n;
 }
 jr=sfs64_stream_finish(&w->stream);
 if(jr!=SJ_OK){sfs64_stream_cancel(&w->stream);return from_data(jr);}
 uint64_t dir_epoch=current.dir_epoch;
 if(!revision){
  struct dc64_edit edit={.op=DC64_CREATE,.entry={*key,id,SD64_FILE}};
  enum dc64_status dr=dc64_hierarchy_apply(d,&w->pair.dw,a,cap,&edit);
  if(dr!=DC64_OK)return from_dir(dr);
  dir_epoch=d->epoch;
 }
 return sp64_commit(b,l,&w->pair,a,cap,current.sequence,dir_epoch,current.data_epoch+1,out);
}

/* Build a manifest snapshot without the exact deleted object. Existing
 * bank validation proves all ranges and IDs; do not modify retained maps. */
static enum sp64_status remove_map(struct sfs64_journal *j,uint64_t id,uint64_t rev,
 uint8_t *dst,size_t *used){
 if(!sfs64_journal_ready(j,true)||j->writer)return SP64_INVALID;
 const uint8_t *p=j->payload[j->active];size_t n=j->length[j->active],at=0;
 if(!sfs64_snapshot_valid(p,n,j->capacity,j->data_start))return SP64_CORRUPT;
 bool found=false;*used=0;
 while(at<n){
  size_t before=at;uint64_t len=get(p+at,4);at+=4;
  struct sfs64_map m;
  if(len>n-at||!sfs64_map_decode(p+at,(size_t)len,j->capacity,j->data_start,&m))return SP64_CORRUPT;
  at+=(size_t)len;
  if(m.file.id==id){
   if(found||m.file.revision!=rev)return SP64_INVALID;
   found=true;continue;
  }
  for(size_t i=before;i<at;i++)dst[(*used)++]=p[i];
 }
 return found?SP64_OK:SP64_INVALID;
}
enum sp64_status sp64_edit(struct bs_service *b,const struct sp64_layout *l,
 struct sp64_edit_workspace *w,struct dc64_node *a,size_t cap,uint64_t expected,
 const struct dc64_edit *e,uint64_t rev,struct sp64_pair *out){
 if(!w||!cap||cap>SIZE_MAX/sizeof *a||!expected||
    !sep(w,sizeof *w,b,sizeof *b)||!sep(w,sizeof *w,l,sizeof *l)||
    !sep(w,sizeof *w,a,cap*sizeof *a)||!sep(w,sizeof *w,out,sizeof *out)||
    !sep(e,sizeof *e,w,sizeof *w)||!sep(e,sizeof *e,a,cap*sizeof *a)||
    !sep(e,sizeof *e,b,sizeof *b)||!sep(e,sizeof *e,l,sizeof *l)||
    !sep(e,sizeof *e,out,sizeof *out)||!sd64_key_valid(&e->entry.key)||
    e->entry.target<=1||
    (e->entry.kind!=SD64_FILE&&e->entry.kind!=SD64_DIRECTORY)||
    (e->op!=DC64_CREATE&&e->op!=DC64_REMOVE&&e->op!=DC64_RENAME)||
    (e->op==DC64_CREATE&&e->entry.kind!=SD64_DIRECTORY)||
    (e->entry.kind==SD64_FILE?!rev:rev!=0)||
    (e->op==DC64_RENAME&&!sd64_key_valid(&e->destination)))return SP64_INVALID;
 struct sp64_pair cur;
 enum sp64_status rc=sp64_mount(b,l,&w->pair,a,cap,&cur);
 if(rc!=SP64_OK)return rc;
 if(cur.sequence!=expected)return SP64_INVALID;
 rc=sp64_prepare(b,l,&w->pair,a,cap,&cur);
 if(rc!=SP64_OK)return rc;
 if(cur.sequence!=expected)return SP64_INVALID;
 bool deleting=e->op==DC64_REMOVE&&e->entry.kind==SD64_FILE;
 if(cur.sequence==UINT64_MAX||cur.dir_epoch==UINT64_MAX||
    (deleting&&cur.data_epoch==UINT64_MAX))return SP64_FULL;
 struct sfs64_journal *j=&w->pair.data;
 size_t bytes=0;
 if(e->entry.kind==SD64_FILE){
  struct sfs64_file f;
  if(sfs64_stream_stat(j,e->entry.target,&f)!=SJ_OK||f.revision!=rev)return SP64_INVALID;
  if(deleting){
   rc=remove_map(j,e->entry.target,rev,w->payload,&bytes);
   if(rc!=SP64_OK)return rc;
  }
 }
 /* Validate hierarchy and exact name/object and publish inactive directory
  * first. It remains invisible until the paired receipt is durable. */
 enum dc64_status dr=dc64_hierarchy_apply(&w->pair.dir,&w->pair.dw,a,cap,e);
 if(dr!=DC64_OK)return from_dir(dr);
 if(deleting){
  enum sj_status jr=sfs64_journal_publish(j,w->payload,bytes);
  if(jr!=SJ_OK)return from_data(jr);
 }
 return sp64_commit(b,l,&w->pair,a,cap,cur.sequence,cur.dir_epoch+1,
                    cur.data_epoch+(deleting?1u:0u),out);
}
