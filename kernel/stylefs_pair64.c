
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
