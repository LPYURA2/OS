#include <styleos/stylefs_inodecommit64.h>
static const uint8_t magic[8]={'S','F','6','4','I','C','0','1'};
struct receipt {uint64_t epoch,pages,count;};
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){for(size_t i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
static bool zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p,size_t n){
 uint32_t c=UINT32_MAX;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned k=0;k<8;k++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}return ~c;
}
static bool geom(uint64_t n,uint64_t cap,uint64_t start){return n>=18&&(n-2)%16==0&&start&&start<cap;}
static uint64_t banks(uint64_t n){return (n-2)/2;}
static uint64_t base(uint64_t n,unsigned slot){return 2+slot*banks(n);}
static bool grant(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t n,bool write){
 if(!b||b->busy||!owner||!token)return false;
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=&b->grants[i];
  if(g->token!=token)continue;
  if(g->owner!=owner||g->length!=n||g->device>=BS_DEVICES||!(g->rights&BS_READ)||(write&&!(g->rights&BS_WRITE)))return false;
  const struct bs_device *d=&b->devices[g->device];
  return g->generation&&d->generation==g->generation&&!d->recovery&&
   g->start<d->backend.sectors&&n<=d->backend.sectors-g->start&&
   (!write||(d->backend.writable&&!d->backend.system_media));
 }return false;
}
static void encode(uint8_t p[512],uint64_t n,uint64_t cap,uint64_t start,unsigned slot,struct receipt x){
 for(unsigned i=0;i<512;i++)p[i]=0;
 for(unsigned i=0;i<8;i++)p[i]=magic[i];
 put(p+8,x.epoch,8);put(p+16,x.pages,8);put(p+24,x.count,8);
 put(p+32,n,8);put(p+40,cap,8);put(p+48,start,8);put(p+56,slot,4);put(p+60,crc(p,60),4);
}
static bool decode(const uint8_t p[512],uint64_t n,uint64_t cap,uint64_t start,unsigned slot,struct receipt *x){
 if(!equal(p,magic,8)||get(p+32,8)!=n||get(p+40,8)!=cap||get(p+48,8)!=start||
    get(p+56,4)!=slot||get(p+60,4)!=crc(p,60)||!zero(p+64,448))return false;
 struct receipt a={get(p+8,8),get(p+16,8),get(p+24,8)};
 if(!a.epoch||a.pages>banks(n)/8||a.pages>a.count||(!a.pages&&a.count))return false;
 *x=a;return true;
}
static enum ic64_status write(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t at,const uint8_t p[512]){
 uint8_t check[512];
 return bs_write_sector(b,owner,token,at,p)==BS_OK&&bs_read_sector(b,owner,token,at,check)==BS_OK&&equal(p,check,512)?IC64_OK:IC64_UNCERTAIN;
}
enum ic64_status ic64_mount(struct ic64_state *s,struct bs_service *b,uint64_t owner,uint64_t token,
 uint64_t n,uint64_t cap,uint64_t start,struct ic64_workspace *w){
 if(!sep(s,sizeof *s,b,sizeof *b)||!sep(s,sizeof *s,w,sizeof *w)||!sep(w,sizeof *w,b,sizeof *b))return IC64_INVALID;
 *s=(struct ic64_state){0};
 if(!geom(n,cap,start)||!grant(b,owner,token,n,false))return IC64_INVALID;
 s->sectors=n;
 for(unsigned slot=0;slot<2;slot++){
  uint8_t p[512];struct receipt x;
  if(bs_read_sector(b,owner,token,slot,p)!=BS_OK){s->table.valid=false;return IC64_IO;}
  if(!decode(p,n,cap,start,slot,&x))continue;
  struct it64_geometry g={x.epoch,base(n,slot),x.pages,cap,start};
  struct it64_state candidate;
  enum it64_status rc=it64_mount(&candidate,b,owner,token,&g,w->page);
  if(rc==IT64_IO){s->table.valid=false;return IC64_IO;}
  if(rc!=IT64_OK||candidate.count!=x.count)continue;
  if(s->table.valid&&s->table.geometry.epoch==x.epoch){s->table.valid=false;return IC64_CORRUPT;}
  if(!s->table.valid||x.epoch>s->table.geometry.epoch){s->table=candidate;s->active=slot;}
 }
 return s->table.valid?IC64_OK:IC64_CORRUPT;
}
#ifdef STYLEFS_HOST_FORMAT
enum ic64_status ic64_provision(struct bs_service *b,uint64_t owner,uint64_t token,
 uint64_t n,uint64_t cap,uint64_t start){
 if(!geom(n,cap,start)||!grant(b,owner,token,n,true))return IC64_INVALID;
 uint8_t p[512];
 for(unsigned i=0;i<2;i++){
  if(bs_read_sector(b,owner,token,i,p)!=BS_OK)return IC64_IO;
  if(!zero(p,512))return IC64_INVALID;
 }
 encode(p,n,cap,start,0,(struct receipt){1,0,0});
 return write(b,owner,token,0,p);
}
#endif
static enum ic64_status emit(struct ic64_state *s,struct ic64_workspace *w,struct it64_geometry *g,uint64_t ordinal,unsigned n){
 if(ordinal>=g->pages)return IC64_FULL;
 if(!it64_encode(w->page,g,ordinal,w->blobs,n))return IC64_INVALID;
 uint64_t at=g->start+ordinal*8;
 for(unsigned i=0;i<8;i++)
  if(write(s->table.block,s->table.owner,s->table.token,at+i,w->page+i*512)!=IC64_OK){s->table.valid=false;return IC64_UNCERTAIN;}
 return IC64_OK;
}
enum ic64_status ic64_publish(struct ic64_state *s,struct ic64_workspace *w,uint64_t count,ic64_source next,void *ctx){
 if(!sep(s,sizeof *s,w,sizeof *w)||!s->table.valid||s->active>1||
    !sep(s,sizeof *s,s->table.block,sizeof *s->table.block)||
    !sep(w,sizeof *w,s->table.block,sizeof *s->table.block)||(count&&!next))return IC64_INVALID;
 struct it64_state *t=&s->table;uint64_t n=s->sectors,cap=t->geometry.capacity,start=t->geometry.data_start;
 if(!geom(n,cap,start)||!grant(t->block,t->owner,t->token,n,true))return IC64_INVALID;
 if(t->geometry.epoch==UINT64_MAX)return IC64_FULL;
 uint64_t max=banks(n)/8;
 if(count/IT64_MAX_RECORDS+(count%IT64_MAX_RECORDS!=0)>max)return IC64_FULL;
 /* Re-read durable state before touching inactive bank; reject stale copy. */
 struct ic64_state fresh;enum ic64_status rc=ic64_mount(&fresh,t->block,t->owner,t->token,n,cap,start,w);
 if(rc!=IC64_OK){t->valid=false;return rc;}
 if(fresh.active!=s->active||fresh.table.geometry.epoch!=t->geometry.epoch||
    fresh.table.geometry.start!=t->geometry.start||fresh.table.geometry.pages!=t->geometry.pages||
    fresh.table.count!=t->count){t->valid=false;return IC64_STALE;}
 unsigned target=s->active^1u;
 struct it64_geometry g={t->geometry.epoch+1,base(n,target),max,cap,start};
 uint8_t p[512]={0};
 if(write(t->block,t->owner,t->token,target,p)!=IC64_OK){t->valid=false;return IC64_UNCERTAIN;}
 uint64_t pages=0,prev=0;unsigned records=0;size_t used=0;
 for(uint64_t i=0;i<count;i++){
  size_t bytes=0;struct sfs64_map map;
  if(!next(ctx,i,w->next,IT64_PAGE-IT64_HEADER-4,&bytes))return IC64_CANCELLED;
  if(bytes>IT64_PAGE-IT64_HEADER-4||!sfs64_map_decode(w->next,bytes,cap,start,&map)||map.file.id<=prev)return IC64_INVALID;
  prev=map.file.id;
  if(records==IT64_MAX_RECORDS||bytes+4>IT64_PAGE-IT64_HEADER-used){
   rc=emit(s,w,&g,pages,records);if(rc!=IC64_OK)return rc;
   ++pages;records=0;used=0;
  }
  size_t offset=used;
  for(size_t k=0;k<bytes;k++)w->packed[offset+k]=w->next[k];
  w->blobs[records++]=(struct it64_blob){w->packed+offset,bytes};used+=bytes+4;
 }
 if(records){rc=emit(s,w,&g,pages,records);if(rc!=IC64_OK)return rc;++pages;}
 g.pages=pages;
 struct it64_state candidate;
 if(it64_mount(&candidate,t->block,t->owner,t->token,&g,w->page)!=IT64_OK||candidate.count!=count){t->valid=false;return IC64_UNCERTAIN;}
 encode(p,n,cap,start,target,(struct receipt){g.epoch,pages,count});
 if(write(t->block,t->owner,t->token,target,p)!=IC64_OK){t->valid=false;return IC64_UNCERTAIN;}
 s->table=candidate;s->active=target;return IC64_OK;
}
