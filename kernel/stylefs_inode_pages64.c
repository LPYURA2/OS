
#include <styleos/stylefs_inode_pages64.h>
static const uint8_t magic[8]={'S','F','6','4','I','P','G','1'};
static void zero(uint8_t *p,size_t n){for(size_t i=0;i<n;i++)p[i]=0;}
static bool zeros(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static bool equal(const uint8_t *p,const uint8_t *q,size_t n){for(size_t i=0;i<n;i++)if(p[i]!=q[i])return false;return true;}
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p){
 uint32_t c=UINT32_MAX;
 for(unsigned i=0;i<IP64_PAGE;i++){c^=i>=48&&i<52?0:p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
 return ~c;
}
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool geometry(struct bs_service *b,uint64_t owner,uint64_t token,const struct ip64_root *r,bool write){
 if(!b||b->busy||!owner||!token||!r||!r->epoch||!r->sectors||r->sectors%8||
    r->start>UINT64_MAX-r->sectors||r->pages>r->sectors/8||
    (!!r->files!=!!r->pages)||r->pages>r->files||!r->data_start||r->data_start>=r->data_capacity)return false;
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=&b->grants[i];
  if(g->token!=token)continue;
  if(g->owner!=owner||g->device>=BS_DEVICES||!(g->rights&BS_READ)||
     (write&&!(g->rights&BS_WRITE))||r->start>g->length||r->sectors>g->length-r->start)return false;
  const struct bs_device *d=&b->devices[g->device];
  return g->generation&&g->generation==d->generation&&!d->recovery&&
   (!write||(d->backend.writable&&!d->backend.system_media));
 }return false;
}
static bool args(struct bs_service *b,const struct ip64_root *r,struct ip64_workspace *w){
 return sep(b,sizeof *b,r,sizeof *r)&&sep(w,sizeof *w,r,sizeof *r)&&sep(w,sizeof *w,b,sizeof *b);
}
static enum ip64_status flush_page(struct bs_service *b,uint64_t owner,uint64_t token,
 const struct ip64_root *r,struct ip64_workspace *w,uint64_t index,size_t used,unsigned count,uint64_t first,uint64_t last){
 uint8_t *p=w->page;
 for(unsigned i=0;i<8;i++)p[i]=magic[i];
 put(p+8,r->epoch,8);put(p+16,index,8);put(p+24,first,8);put(p+32,last,8);
 put(p+40,used,4);put(p+44,count,4);put(p+48,crc(p),4);
 uint64_t at=r->start+index*8;uint8_t check[512];
 for(unsigned i=0;i<8;i++){
  if(bs_write_sector(b,owner,token,at+i,p+i*512)!=BS_OK||
     bs_read_sector(b,owner,token,at+i,check)!=BS_OK||!equal(check,p+i*512,512))return IP64_UNCERTAIN;
 }
 return IP64_OK;
}
enum ip64_status ip64_build(struct bs_service *b,uint64_t owner,uint64_t token,
 const struct ip64_root *r,uint64_t count,ip64_source next,void *ctx,
 struct ip64_workspace *w,struct ip64_root *out){
 if(!args(b,r,w)||!sep(out,sizeof *out,r,sizeof *r)||!sep(out,sizeof *out,w,sizeof *w)||
    !sep(out,sizeof *out,b,sizeof *b)||!geometry(b,owner,token,r,true)||r->pages||r->files||(count&&!next))return IP64_INVALID;
 /* Even an empty map needs 68 bytes. Reject impossible counts without writes. */
 uint64_t maxpages=r->sectors/8;
 enum { MAX_PER_PAGE=(IP64_PAGE-IP64_HEADER)/(4+SFS64_MAP_HEADER) };
 if(count/MAX_PER_PAGE+(count%MAX_PER_PAGE!=0)>maxpages)return IP64_FULL;
 uint64_t index=0,first=0,last=0;size_t used=IP64_HEADER;unsigned items=0;
 zero(w->page,IP64_PAGE);
 for(uint64_t i=0;i<count;i++){
  size_t n=0;struct sfs64_map m;
  if(!next(ctx,i,w->map,sizeof w->map,&n))return IP64_CANCELLED;
  if(n>sizeof w->map||!sfs64_map_decode(w->map,n,r->data_capacity,r->data_start,&m)||m.file.id<=last)return IP64_INVALID;
  if(n+4>IP64_PAGE-used){
   if(index>=maxpages)return IP64_FULL;
   enum ip64_status rc=flush_page(b,owner,token,r,w,index,used,items,first,last);
   if(rc!=IP64_OK)return rc;
   index++;used=IP64_HEADER;items=0;zero(w->page,IP64_PAGE);
  }
  if(index>=maxpages)return IP64_FULL;
  if(!items)first=m.file.id;
  last=m.file.id;
  put(w->page+used,n,4);used+=4;
  for(size_t k=0;k<n;k++)w->page[used+k]=w->map[k];
  used+=n;items++;
 }
 if(items){
  enum ip64_status rc=flush_page(b,owner,token,r,w,index,used,items,first,last);
  if(rc!=IP64_OK)return rc;
  index++;
 }
 *out=*r;out->pages=index;out->files=count;return IP64_OK;
}
struct page_info {uint64_t first,last;unsigned count;size_t used;};
static enum ip64_status load(struct bs_service *b,uint64_t owner,uint64_t token,
 const struct ip64_root *r,struct ip64_workspace *w,uint64_t index,struct page_info *info){
 uint8_t *p=w->page;uint64_t at=r->start+index*8;
 for(unsigned i=0;i<8;i++)if(bs_read_sector(b,owner,token,at+i,p+i*512)!=BS_OK)return IP64_IO;
 uint64_t used=get(p+40,4),count=get(p+44,4);
 if(!equal(p,magic,8)||get(p+8,8)!=r->epoch||get(p+16,8)!=index||
    used<IP64_HEADER||used>IP64_PAGE||!count||count>(used-IP64_HEADER)/(4+SFS64_MAP_HEADER)||
    get(p+48,4)!=crc(p)||!zeros(p+52,12)||!zeros(p+used,IP64_PAGE-used))return IP64_CORRUPT;
 size_t pos=IP64_HEADER;uint64_t first=0,last=0;
 for(unsigned i=0;i<count;i++){
  if(used-pos<4)return IP64_CORRUPT;
  uint64_t len=get(p+pos,4);pos+=4;struct sfs64_map m;
  if(len>used-pos||!sfs64_map_decode(p+pos,(size_t)len,r->data_capacity,r->data_start,&m)||m.file.id<=last)return IP64_CORRUPT;
  if(!i)first=m.file.id;
  last=m.file.id;pos+=(size_t)len;
 }
 if(pos!=used||first!=get(p+24,8)||last!=get(p+32,8))return IP64_CORRUPT;
 *info=(struct page_info){first,last,(unsigned)count,(size_t)used};return IP64_OK;
}
static void swap(struct sfs64_run *a,struct sfs64_run *b){struct sfs64_run t=*a;*a=*b;*b=t;}
static void sift(struct sfs64_run *a,size_t n,size_t i){
 while(i<n/2){size_t k=i*2+1;if(k+1<n&&a[k].first<a[k+1].first)k++;
  if(a[i].first>=a[k].first)break;
  swap(a+i,a+k);i=k;
 }
}
static void sort(struct sfs64_run *a,size_t n){
 for(size_t i=n/2;i;i--)sift(a,n,i-1);
 for(size_t i=n;i>1;i--){swap(a,a+i-1);sift(a,i-1,0);}
}
static enum ip64_status validate(struct bs_service *b,uint64_t owner,uint64_t token,
 const struct ip64_root *r,struct ip64_workspace *w,struct sfs64_run *runs,size_t cap,size_t *count){
 if(!args(b,r,w)||!geometry(b,owner,token,r,false)||cap>SIZE_MAX/sizeof *runs||
    (cap&&(!sep(runs,cap*sizeof *runs,b,sizeof *b)||!sep(runs,cap*sizeof *runs,r,sizeof *r)||
     !sep(runs,cap*sizeof *runs,w,sizeof *w))))return IP64_INVALID;
 uint64_t last=0,total=0;size_t used=0;
 for(uint64_t i=0;i<r->pages;i++){
  struct page_info info;enum ip64_status rc=load(b,owner,token,r,w,i,&info);
  if(rc!=IP64_OK)return rc;
  if(info.first<=last||info.count>r->files-total)return IP64_CORRUPT;
  last=info.last;total+=info.count;
  size_t at=IP64_HEADER;
  while(at<info.used){
   uint64_t len=get(w->page+at,4);at+=4;struct sfs64_map m;
   if(!sfs64_map_decode(w->page+at,(size_t)len,r->data_capacity,r->data_start,&m))return IP64_CORRUPT;
   if(m.count>cap-used)return IP64_FULL;
   for(unsigned k=0;k<m.count;k++){
    const uint8_t *p=m.records+k*SFS64_MAP_RECORD;
    runs[used++]=(struct sfs64_run){get(p,8),get(p+8,8)};
   }
   at+=(size_t)len;
  }
 }
 if(total!=r->files)return IP64_CORRUPT;
 sort(runs,used);
 for(size_t i=1;i<used;i++)if(runs[i].first<runs[i-1].first+runs[i-1].sectors)return IP64_CORRUPT;
 if(count)*count=used;
 return IP64_OK;
}
enum ip64_status ip64_validate(struct bs_service *b,uint64_t owner,uint64_t token,
 const struct ip64_root *r,struct ip64_workspace *w,struct sfs64_run *runs,size_t cap){
 return validate(b,owner,token,r,w,runs,cap,0);
}
enum ip64_status ip64_inventory(struct bs_service *b,uint64_t owner,uint64_t token,
 const struct ip64_root *r,struct ip64_workspace *w,struct sfs64_run *runs,size_t cap,size_t *count){
 if(cap>SIZE_MAX/sizeof *runs||!sep(count,sizeof *count,b,sizeof *b)||
    !sep(count,sizeof *count,r,sizeof *r)||!sep(count,sizeof *count,w,sizeof *w)||
    (cap&&!sep(count,sizeof *count,runs,cap*sizeof *runs)))return IP64_INVALID;
 return validate(b,owner,token,r,w,runs,cap,count);
}
enum ip64_status ip64_stat(struct bs_service *b,uint64_t owner,uint64_t token,
 const struct ip64_root *r,uint64_t id,struct ip64_workspace *w,struct sfs64_file *out){
 if(!args(b,r,w)||!geometry(b,owner,token,r,false)||!id||
    !sep(out,sizeof *out,w,sizeof *w)||!sep(out,sizeof *out,r,sizeof *r)||!sep(out,sizeof *out,b,sizeof *b))return IP64_INVALID;
 uint64_t lo=0,hi=r->pages;
 while(lo<hi){
  uint64_t mid=lo+(hi-lo)/2;struct page_info info;
  enum ip64_status rc=load(b,owner,token,r,w,mid,&info);
  if(rc!=IP64_OK)return rc;
  if(id<info.first)hi=mid;
  else if(id>info.last)lo=mid+1;
  else{
   size_t at=IP64_HEADER;
   while(at<info.used){
    uint64_t len=get(w->page+at,4);at+=4;struct sfs64_map m;
    if(!sfs64_map_decode(w->page+at,(size_t)len,r->data_capacity,r->data_start,&m))return IP64_CORRUPT;
    if(m.file.id==id){*out=m.file;return IP64_OK;}
    if(m.file.id>id)break;
    at+=(size_t)len;
   }
   return IP64_MISSING;
  }
 }
 return IP64_MISSING;
}

enum ip64_status ip64_export(struct bs_service *b,uint64_t owner,uint64_t token,
 const struct ip64_root *r,uint64_t id,struct ip64_workspace *w,size_t *length){
 if(!args(b,r,w)||!geometry(b,owner,token,r,false)||!id||
    !sep(length,sizeof *length,w,sizeof *w)||!sep(length,sizeof *length,r,sizeof *r)||
    !sep(length,sizeof *length,b,sizeof *b))return IP64_INVALID;
 uint64_t lo=0,hi=r->pages;
 while(lo<hi){
  uint64_t mid=lo+(hi-lo)/2;struct page_info info;
  enum ip64_status rc=load(b,owner,token,r,w,mid,&info);
  if(rc!=IP64_OK)return rc;
  if(id<info.first)hi=mid;
  else if(id>info.last)lo=mid+1;
  else{
   size_t at=IP64_HEADER;
   while(at<info.used){
    uint64_t len=get(w->page+at,4);at+=4;struct sfs64_map m;
    if(len>sizeof w->map||!sfs64_map_decode(w->page+at,(size_t)len,r->data_capacity,r->data_start,&m))return IP64_CORRUPT;
    if(m.file.id==id){
     for(size_t i=0;i<len;i++)w->map[i]=w->page[at+i];
     *length=(size_t)len;return IP64_OK;
    }
    if(m.file.id>id)break;
    at+=(size_t)len;
   }
   return IP64_MISSING;
  }
 }
 return IP64_MISSING;
}
