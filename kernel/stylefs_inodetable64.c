#include <styleos/stylefs_inodetable64.h>
static const uint8_t magic[8]={'S','F','6','4','I','N','O','1'};
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p){
 uint32_t c=UINT32_MAX;
 for(unsigned i=0;i<IT64_PAGE;i++){
  c^=i>=32&&i<36?0:p[i];
  for(unsigned k=0;k<8;k++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);
 }return ~c;
}
static bool geom(const struct it64_geometry *g){
 return g&&g->epoch&&g->data_start&&g->data_start<g->capacity&&
 g->pages<=(UINT64_MAX-g->start)/IT64_SECTORS;
}
static bool admitted(struct bs_service *b,uint64_t owner,uint64_t token,const struct it64_geometry *g){
 if(!b||b->busy||!owner||!token||!geom(g))return false;
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *a=&b->grants[i];
  if(a->token!=token)continue;
  if(a->owner!=owner||!(a->rights&BS_READ)||a->device>=BS_DEVICES||
     g->start>a->length||g->pages*IT64_SECTORS>a->length-g->start)return false;
  const struct bs_device *d=&b->devices[a->device];
  return a->generation&&a->generation==d->generation&&!d->recovery&&
   a->start<d->backend.sectors&&a->length<=d->backend.sectors-a->start;
 }return false;
}
bool it64_encode(uint8_t p[IT64_PAGE],const struct it64_geometry *g,uint64_t ordinal,
 const struct it64_blob *blobs,unsigned n){
 if(!geom(g)||ordinal>=g->pages||!n||n>IT64_MAX_RECORDS||
    !sep(p,IT64_PAGE,g,sizeof *g)||!sep(p,IT64_PAGE,blobs,n*sizeof *blobs))return false;
 size_t at=IT64_HEADER;uint64_t prev=0;
 for(unsigned i=0;i<n;i++){
  struct sfs64_map m;size_t bytes=blobs[i].length;
  if(at>IT64_PAGE-4||bytes>IT64_PAGE-at-4||
     !sep(p,IT64_PAGE,blobs[i].bytes,bytes)||
     !sfs64_map_decode(blobs[i].bytes,bytes,g->capacity,g->data_start,&m)||
     m.file.id<=prev)return false;
  prev=m.file.id;at+=4+bytes;
 }
 for(unsigned i=0;i<IT64_PAGE;i++)p[i]=0;
 for(unsigned i=0;i<8;i++)p[i]=magic[i];
 put(p+8,g->epoch,8);put(p+16,ordinal,8);put(p+24,n,4);put(p+28,at,4);
 put(p+40,g->capacity,8);put(p+48,g->data_start,8);
 at=IT64_HEADER;
 for(unsigned i=0;i<n;i++){
  put(p+at,blobs[i].length,4);at+=4;
  for(size_t j=0;j<blobs[i].length;j++)p[at+j]=blobs[i].bytes[j];
  at+=blobs[i].length;
 }
 put(p+32,crc(p),4);return true;
}
struct page_info {uint64_t first,last;unsigned count;bool found;struct sfs64_map map;};
static bool scan(const uint8_t p[IT64_PAGE],const struct it64_geometry *g,uint64_t ordinal,
 uint64_t target,struct page_info *out){
 for(unsigned i=0;i<8;i++)if(p[i]!=magic[i])return false;
 uint64_t n=get(p+24,4),used=get(p+28,4);
 if(get(p+8,8)!=g->epoch||get(p+16,8)!=ordinal||
    !n||n>IT64_MAX_RECORDS||used<IT64_HEADER||used>IT64_PAGE||
    get(p+32,4)!=crc(p)||!zero(p+36,4)||get(p+40,8)!=g->capacity||
    get(p+48,8)!=g->data_start||!zero(p+56,8)||!zero(p+used,IT64_PAGE-used))return false;
 struct page_info info={.count=(unsigned)n};size_t at=IT64_HEADER;
 for(unsigned i=0;i<n;i++){
  if(used-at<4)return false;
  size_t bytes=(size_t)get(p+at,4);at+=4;
  struct sfs64_map m;
  if(bytes>used-at||!sfs64_map_decode(p+at,bytes,g->capacity,g->data_start,&m)||m.file.id<=info.last)return false;
  if(!i)info.first=m.file.id;
  info.last=m.file.id;at+=bytes;
  if(target==m.file.id){info.found=true;info.map=m;}
 }
 if(at!=used)return false;
 *out=info;return true;
}
static enum it64_status read_page(struct it64_state *s,uint64_t page,uint8_t p[IT64_PAGE],
 uint64_t id,struct page_info *info){
 if(page>=s->geometry.pages)return IT64_INVALID;
 uint64_t at=s->geometry.start+page*IT64_SECTORS;
 for(unsigned i=0;i<IT64_SECTORS;i++)
  if(bs_read_sector(s->block,s->owner,s->token,at+i,p+i*512)!=BS_OK){s->valid=false;return IT64_IO;}
 if(!scan(p,&s->geometry,page,id,info)){s->valid=false;return IT64_CORRUPT;}
 return IT64_OK;
}
enum it64_status it64_mount(struct it64_state *s,struct bs_service *b,uint64_t owner,uint64_t token,
 const struct it64_geometry *g,uint8_t p[IT64_PAGE]){
 if(!sep(s,sizeof *s,b,sizeof *b)||!sep(s,sizeof *s,g,sizeof *g)||
    !sep(s,sizeof *s,p,IT64_PAGE)||!sep(p,IT64_PAGE,b,sizeof *b)||!sep(p,IT64_PAGE,g,sizeof *g))return IT64_INVALID;
 *s=(struct it64_state){0};
 if(!admitted(b,owner,token,g))return IT64_INVALID;
 s->block=b;s->owner=owner;s->token=token;s->geometry=*g;
 uint64_t prev=0,total=0;
 for(uint64_t i=0;i<g->pages;i++){
  struct page_info info;enum it64_status rc=read_page(s,i,p,0,&info);
  if(rc!=IT64_OK)return rc;
  if(info.first<=prev||info.count>UINT64_MAX-total)return IT64_CORRUPT;
  prev=info.last;total+=info.count;
 }
 s->count=total;s->valid=true;return IT64_OK;
}
enum it64_status it64_lookup(struct it64_state *s,uint64_t epoch,uint64_t id,
 uint8_t p[IT64_PAGE],struct sfs64_map *out){
 if(!s||!s->valid||s->geometry.epoch!=epoch)return IT64_STALE;
 if(!id||!sep(s,sizeof *s,p,IT64_PAGE)||!sep(s,sizeof *s,out,sizeof *out)||
    !sep(p,IT64_PAGE,out,sizeof *out)||!sep(p,IT64_PAGE,s->block,sizeof *s->block)||
    !sep(out,sizeof *out,s->block,sizeof *s->block))return IT64_INVALID;
 if(!admitted(s->block,s->owner,s->token,&s->geometry)){s->valid=false;return IT64_STALE;}
 uint64_t lo=0,hi=s->geometry.pages;
 while(lo<hi){
  uint64_t mid=lo+(hi-lo)/2;struct page_info info;
  enum it64_status rc=read_page(s,mid,p,id,&info);
  if(rc!=IT64_OK)return rc;
  if(id<info.first)hi=mid;
  else if(id>info.last)lo=mid+1;
  else{
   if(!info.found)return IT64_MISSING;
   *out=info.map;return IT64_OK;
  }
 }
 return IT64_MISSING;
}

enum it64_status it64_walk(struct it64_state *s,uint8_t p[IT64_PAGE],it64_visit visit,void *ctx){
 if(!s||!s->valid)return IT64_STALE;
 if(!visit||!sep(s,sizeof *s,p,IT64_PAGE)||!sep(p,IT64_PAGE,s->block,sizeof *s->block))return IT64_INVALID;
 if(!admitted(s->block,s->owner,s->token,&s->geometry)){s->valid=false;return IT64_STALE;}
 uint64_t prev=0,total=0;
 for(uint64_t i=0;i<s->geometry.pages;i++){
  struct page_info info;enum it64_status rc=read_page(s,i,p,0,&info);
  if(rc!=IT64_OK)return rc;
  if(info.first<=prev||info.count>UINT64_MAX-total){s->valid=false;return IT64_CORRUPT;}
  prev=info.last;total+=info.count;
  size_t at=IT64_HEADER;
  for(unsigned j=0;j<info.count;j++){
   size_t n=(size_t)get(p+at,4);at+=4;
   struct sfs64_map m;
   if(!sfs64_map_decode(p+at,n,s->geometry.capacity,s->geometry.data_start,&m)){s->valid=false;return IT64_CORRUPT;}
   if(!visit(ctx,&m))return IT64_INVALID;
   at+=n;
  }
 }
 if(total!=s->count){s->valid=false;return IT64_CORRUPT;}
 return IT64_OK;
}
