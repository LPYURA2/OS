
#include <styleos/stylefs_inode_root64.h>
static const uint8_t magic[8]={'S','F','6','4','I','R','T','1'};
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){for(size_t i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
static bool zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p,size_t n){
 uint32_t c=UINT32_MAX;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}return ~c;
}
static uint64_t bank_size(uint64_t sectors){return (sectors-2)/2;}
static struct ip64_root root_for(uint64_t n,uint64_t dc,uint64_t ds,unsigned bank,uint64_t epoch){
 return (struct ip64_root){.start=2+bank*bank_size(n),.sectors=bank_size(n),.epoch=epoch,.data_capacity=dc,.data_start=ds};
}
static bool grant(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t n,uint64_t dc,uint64_t ds,bool write){
 if(!b||b->busy||!owner||!token||n<18||(n-2)%16||!ds||ds>=dc)return false;
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=&b->grants[i];
  if(g->token!=token)continue;
  if(g->owner!=owner||g->length!=n||g->device>=BS_DEVICES||!(g->rights&BS_READ)||
    (write&&!(g->rights&BS_WRITE)))return false;
  const struct bs_device *d=&b->devices[g->device];
  return g->generation&&g->generation==d->generation&&!d->recovery&&
   g->start<d->backend.sectors&&n<=d->backend.sectors-g->start&&
   (!write||(d->backend.writable&&!d->backend.system_media));
 }
 return false;
}
static bool buffers(struct ir64_state *s,struct bs_service *b,struct ip64_workspace *w,struct sfs64_run *a,size_t cap){
 if(!sep(s,sizeof *s,b,sizeof *b)||!sep(w,sizeof *w,s,sizeof *s)||
    !sep(w,sizeof *w,b,sizeof *b)||cap>SIZE_MAX/sizeof *a)return false;
 return !cap||(sep(a,cap*sizeof *a,s,sizeof *s)&&sep(a,cap*sizeof *a,b,sizeof *b)&&sep(a,cap*sizeof *a,w,sizeof *w));
}
static void encode(uint8_t p[512],uint64_t n,unsigned bank,const struct ip64_root *r){
 for(unsigned i=0;i<512;i++)p[i]=0;
 for(unsigned i=0;i<8;i++)p[i]=magic[i];
 put(p+8,r->epoch,8);put(p+16,r->pages,8);put(p+24,r->files,8);
 put(p+32,n,8);put(p+40,r->data_capacity,8);put(p+48,r->data_start,8);
 put(p+56,bank,4);put(p+60,crc(p,60),4);
}
static bool decode(const uint8_t p[512],uint64_t n,uint64_t dc,uint64_t ds,unsigned bank,struct ip64_root *r){
 uint64_t e=get(p+8,8),pages=get(p+16,8),files=get(p+24,8);
 if(!equal(p,magic,8)||!e||get(p+32,8)!=n||get(p+40,8)!=dc||get(p+48,8)!=ds||
    get(p+56,4)!=bank||get(p+60,4)!=crc(p,60)||!zero(p+64,448)||
    pages>bank_size(n)/8||pages>files||(!pages!=!files))return false;
 *r=root_for(n,dc,ds,bank,e);r->pages=pages;r->files=files;return true;
}
static enum ip64_status write_header(struct bs_service *b,uint64_t owner,uint64_t token,unsigned bank,const uint8_t p[512]){
 uint8_t check[512];
 if(bs_write_sector(b,owner,token,bank,p)!=BS_OK||
    bs_read_sector(b,owner,token,bank,check)!=BS_OK||!equal(check,p,512))return IP64_UNCERTAIN;
 return IP64_OK;
}
static enum ip64_status mount(struct ir64_state *s,struct bs_service *b,uint64_t owner,uint64_t token,
 uint64_t n,uint64_t dc,uint64_t ds,uint64_t wanted,struct ip64_workspace *w,struct sfs64_run *a,size_t cap){
 if(!buffers(s,b,w,a,cap))return IP64_INVALID;
 *s=(struct ir64_state){0};
 if(!grant(b,owner,token,n,dc,ds,false))return IP64_INVALID;
 struct ip64_root roots[2];bool good[2];
 for(unsigned i=0;i<2;i++){
  uint8_t p[512];
  if(bs_read_sector(b,owner,token,i,p)!=BS_OK)return IP64_IO;
  good[i]=decode(p,n,dc,ds,i,&roots[i]);
 }
 if(good[0]&&good[1]&&roots[0].epoch==roots[1].epoch)return IP64_CORRUPT;
 unsigned first=good[1]&&(!good[0]||roots[1].epoch>roots[0].epoch)?1:0;
 for(unsigned i=0;i<2;i++){
  unsigned bank=first^i;
  if(!good[bank]||(wanted&&roots[bank].epoch!=wanted))continue;
  enum ip64_status rc=ip64_validate(b,owner,token,&roots[bank],w,a,cap);
  if(rc==IP64_CORRUPT)continue;
  if(rc!=IP64_OK)return rc;
  *s=(struct ir64_state){.block=b,.owner=owner,.token=token,.sectors=n,.active=bank,.valid=true,.root=roots[bank]};
  return IP64_OK;
 }
 return IP64_CORRUPT;
}
enum ip64_status ir64_mount(struct ir64_state *s,struct bs_service *b,uint64_t owner,uint64_t token,
 uint64_t n,uint64_t dc,uint64_t ds,struct ip64_workspace *w,struct sfs64_run *a,size_t cap){
 return mount(s,b,owner,token,n,dc,ds,0,w,a,cap);
}
enum ip64_status ir64_mount_epoch(struct ir64_state *s,struct bs_service *b,uint64_t owner,uint64_t token,
 uint64_t n,uint64_t dc,uint64_t ds,uint64_t e,struct ip64_workspace *w,struct sfs64_run *a,size_t cap){
 if(!e)return IP64_INVALID;
 return mount(s,b,owner,token,n,dc,ds,e,w,a,cap);
}
static bool same(const struct ip64_root *a,const struct ip64_root *b){
 return a->start==b->start&&a->sectors==b->sectors&&a->epoch==b->epoch&&a->pages==b->pages&&
 a->files==b->files&&a->data_capacity==b->data_capacity&&a->data_start==b->data_start;
}
enum ip64_status ir64_publish(struct ir64_state *s,struct ip64_workspace *w,struct sfs64_run *a,size_t cap,
 uint64_t count,ip64_source next,void *ctx){
 if(!s||!s->valid||s->active>1||!buffers(s,s->block,w,a,cap)||(count&&!next)||
    !grant(s->block,s->owner,s->token,s->sectors,s->root.data_capacity,s->root.data_start,true))return IP64_INVALID;
 struct ir64_state live;
 enum ip64_status rc=ir64_mount(&live,s->block,s->owner,s->token,s->sectors,
  s->root.data_capacity,s->root.data_start,w,a,cap);
 if(rc!=IP64_OK){s->valid=false;return rc;}
 if(live.active!=s->active||!same(&live.root,&s->root)){s->valid=false;return IP64_INVALID;}
 if(s->root.epoch==UINT64_MAX)return IP64_FULL;
 unsigned target=s->active^1u;
 struct ip64_root g=root_for(s->sectors,s->root.data_capacity,s->root.data_start,target,s->root.epoch+1),newroot;
 /* A lower bound on required pages; no writes for impossible counts. */
 enum { PER=(IP64_PAGE-IP64_HEADER)/(4+SFS64_MAP_HEADER) };
 if(count/PER+(count%PER!=0)>g.sectors/8)return IP64_FULL;
 uint8_t p[512]={0};
 rc=write_header(s->block,s->owner,s->token,target,p);
 if(rc!=IP64_OK){s->valid=false;return rc;}
 rc=ip64_build(s->block,s->owner,s->token,&g,count,next,ctx,w,&newroot);
 if(rc!=IP64_OK){if(rc==IP64_UNCERTAIN||rc==IP64_IO)s->valid=false;return rc;}
 rc=ip64_validate(s->block,s->owner,s->token,&newroot,w,a,cap);
 if(rc!=IP64_OK){if(rc==IP64_IO)s->valid=false;return rc;}
 encode(p,s->sectors,target,&newroot);
 rc=write_header(s->block,s->owner,s->token,target,p);
 if(rc!=IP64_OK){s->valid=false;return rc;}
 s->active=target;s->root=newroot;return IP64_OK;
}
#ifdef STYLEFS_HOST_FORMAT
enum ip64_status ir64_provision(struct bs_service *b,uint64_t owner,uint64_t token,
 uint64_t n,uint64_t dc,uint64_t ds){
 if(!grant(b,owner,token,n,dc,ds,true))return IP64_INVALID;
 uint8_t p[512];
 for(unsigned i=0;i<2;i++){
  if(bs_read_sector(b,owner,token,i,p)!=BS_OK)return IP64_IO;
  if(!zero(p,512))return IP64_INVALID;
 }
 struct ip64_root root=root_for(n,dc,ds,0,1);
 encode(p,n,0,&root);return write_header(b,owner,token,0,p);
}
#endif
