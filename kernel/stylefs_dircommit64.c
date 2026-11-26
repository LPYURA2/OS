#include <styleos/stylefs_dircommit64.h>
static const uint8_t magic[8]={'S','F','6','4','D','C','M','1'};
struct plan {uint64_t pages[SD64_MAX_LEVEL+1],items[SD64_MAX_LEVEL+1],offset[SD64_MAX_LEVEL+1],total;unsigned height;};
static void zero(uint8_t *p,size_t n){for(size_t i=0;i<n;i++)p[i]=0;}
static bool blank(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static bool equal(const uint8_t *p,const uint8_t *q,size_t n){for(size_t i=0;i<n;i++)if(p[i]!=q[i])return false;return true;}
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t c=UINT32_MAX;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}return ~c;}
static bool separate(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool geometry(uint64_t n){return n>=18&&(n-2)%16==0;}
static uint64_t bank_size(uint64_t n){return (n-2)/2;}
static uint64_t base(uint64_t n,unsigned bank){return 2+bank*bank_size(n);}
static bool plan(uint64_t count,uint64_t capacity,struct plan *p){
 *p=(struct plan){0};if(!count)return true;
 for(unsigned l=0;;l++){
  if(l>SD64_MAX_LEVEL)return false;
  p->items[l]=count;p->offset[l]=p->total;
  uint64_t pages=count/SD64_FANOUT+(count%SD64_FANOUT!=0);
  if(pages>capacity-p->total)return false;
  p->pages[l]=pages;p->total+=pages;p->height=l;
  if(pages==1)return true;
  count=pages;
 }
}
static bool grant(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t n,bool write){
 if(!b||b->busy||!owner||!token||!geometry(n))return false;
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=&b->grants[i];
  if(g->token!=token)continue;
  if(g->owner!=owner||g->length!=n||g->device>=BS_DEVICES||!(g->rights&BS_READ)||(write&&!(g->rights&BS_WRITE)))return false;
  const struct bs_device *d=&b->devices[g->device];
  return g->generation&&d->generation==g->generation&&!d->recovery&&
   g->start<d->backend.sectors&&n<=d->backend.sectors-g->start&&
   (!write||(d->backend.writable&&!d->backend.system_media));
 }
 return false;
}
static void header(uint8_t p[512],uint64_t n,unsigned bank,uint64_t epoch,uint64_t count){
 zero(p,512);for(unsigned i=0;i<8;i++)p[i]=magic[i];
 put(p+8,epoch,8);put(p+16,count,8);put(p+24,n,8);put(p+32,bank,4);put(p+36,crc(p,36),4);
}
static bool decode_header(const uint8_t p[512],uint64_t n,unsigned bank,uint64_t *epoch,uint64_t *count){
 if(!equal(p,magic,8)||!get(p+8,8)||get(p+24,8)!=n||get(p+32,4)!=bank||
    get(p+36,4)!=crc(p,36)||!blank(p+40,472))return false;
 *epoch=get(p+8,8);*count=get(p+16,8);return true;
}
static enum dc64_status write_sector(struct dc64_state *s,uint64_t at,const uint8_t p[512]){
 uint8_t check[512];
 if(bs_write_sector(s->block,s->owner,s->token,at,p)!=BS_OK||
    bs_read_sector(s->block,s->owner,s->token,at,check)!=BS_OK||!equal(p,check,512)){
  s->valid=false;return DC64_UNCERTAIN;
 }return DC64_OK;
}
static enum dc64_status load(struct dc64_state *s,struct dc64_workspace *w,uint64_t at,
                             uint64_t epoch,unsigned level,struct sd64_record *out,unsigned *n){
 for(unsigned i=0;i<SD64_SECTORS;i++)
  if(bs_read_sector(s->block,s->owner,s->token,at+i,w->page+i*512)!=BS_OK)return DC64_IO;
 return sd64_decode_page(w->page,epoch,level,out,n)?DC64_OK:DC64_CORRUPT;
}
static unsigned expected(const struct plan *p,unsigned level,uint64_t page){
 uint64_t remain=p->items[level]-page*SD64_FANOUT;
 return remain<SD64_FANOUT?(unsigned)remain:SD64_FANOUT;
}
static uint64_t location(uint64_t start,const struct plan *p,unsigned level,uint64_t page){
 return start+(p->offset[level]+page)*SD64_SECTORS;
}
/* Scan all pages; compare every internal entry to the exact child's max.
 * A deterministic layout eliminates aliases, cycles, omitted children.
 * Only two page-sized record arrays and one page buffer are required. */
static enum dc64_status validate(struct dc64_state *s,struct dc64_workspace *w,unsigned bank,
                                 uint64_t epoch,const struct plan *p){
 uint64_t start=base(s->sectors,bank);
 for(unsigned l=0;p->total&&l<=p->height;l++){
  struct sd64_key prev={0};bool have=false;
  for(uint64_t i=0;i<p->pages[l];i++){
   unsigned n=0;enum dc64_status rc=load(s,w,location(start,p,l,i),epoch,l,w->records,&n);
   if(rc!=DC64_OK)return rc;
   if(n!=expected(p,l,i))return DC64_CORRUPT;
   for(unsigned k=0;k<n;k++){
    const struct sd64_record *r=&w->records[k];
    if(have&&sd64_compare(&prev,&r->key)>=0)return DC64_CORRUPT;
    prev=r->key;have=true;
    if(l){
     uint64_t child=i*SD64_FANOUT+k,at=location(start,p,l-1,child);
     if(r->target!=at)return DC64_CORRUPT;
     unsigned cn=0;rc=load(s,w,at,epoch,l-1,w->child,&cn);
     if(rc!=DC64_OK)return rc;
     if(cn!=expected(p,l-1,child)||sd64_compare(&r->key,&w->child[cn-1].key)!=0)return DC64_CORRUPT;
    }
   }
  }
 }return DC64_OK;
}
static void select_bank(struct dc64_state *s,unsigned bank,uint64_t epoch,uint64_t count,const struct plan *p){
 uint64_t start=base(s->sectors,bank);
 s->active=bank;s->epoch=epoch;s->count=count;s->valid=true;
 s->root=(struct sd64_root){p->total?location(start,p,p->height,0):0,epoch,start,p->total*SD64_SECTORS,p->height};
}
enum dc64_status dc64_mount(struct dc64_state *s,struct bs_service *b,uint64_t owner,
                             uint64_t token,uint64_t n,struct dc64_workspace *w){
 if(!separate(s,sizeof *s,w,sizeof *w)||!separate(s,sizeof *s,b,sizeof *b)||
    !separate(w,sizeof *w,b,sizeof *b))return DC64_INVALID;
 *s=(struct dc64_state){0};
 if(!grant(b,owner,token,n,false))return DC64_INVALID;
 s->block=b;s->owner=owner;s->token=token;s->sectors=n;
 for(unsigned bank=0;bank<2;bank++){
  uint8_t h[512];uint64_t epoch,count;struct plan p;
  if(bs_read_sector(b,owner,token,bank,h)!=BS_OK){s->valid=false;return DC64_IO;}
  if(!decode_header(h,n,bank,&epoch,&count)||!plan(count,bank_size(n)/8,&p))continue;
  enum dc64_status rc=validate(s,w,bank,epoch,&p);
  if(rc==DC64_IO){s->valid=false;return rc;}
  if(rc!=DC64_OK)continue;
  if(s->valid&&epoch==s->epoch){s->valid=false;return DC64_CORRUPT;}
  if(!s->valid||epoch>s->epoch)select_bank(s,bank,epoch,count,&p);
 }
 return s->valid?DC64_OK:DC64_CORRUPT;
}
#ifdef STYLEFS_HOST_FORMAT
enum dc64_status dc64_provision(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t n){
 if(!grant(b,owner,token,n,true))return DC64_INVALID;
 uint8_t p[512];
 for(unsigned i=0;i<2;i++){
  if(bs_read_sector(b,owner,token,i,p)!=BS_OK)return DC64_IO;
  if(!blank(p,512))return DC64_INVALID;
 }
 struct dc64_state s={.block=b,.owner=owner,.token=token,.sectors=n};
 header(p,n,0,1,0);return write_sector(&s,0,p);
}
#endif
enum dc64_status dc64_publish(struct dc64_state *s,struct dc64_workspace *w,uint64_t count,dc64_next next,void *ctx){
 if(!separate(s,sizeof *s,w,sizeof *w)||!s->valid||s->active>1||!s->epoch||
    !separate(s,sizeof *s,s->block,sizeof *s->block)||
    !separate(w,sizeof *w,s->block,sizeof *s->block)||
    !grant(s->block,s->owner,s->token,s->sectors,true)||(count&&!next))return DC64_INVALID;
 struct plan p;
 if(s->epoch==UINT64_MAX||!plan(count,bank_size(s->sectors)/8,&p))return DC64_FULL;
 unsigned target=s->active^1u;uint64_t epoch=s->epoch+1,start=base(s->sectors,target);
 uint8_t h[512]={0};
 /* Durable invalidation precedes reuse of the inactive bank. */
 if(write_sector(s,target,h)!=DC64_OK)return DC64_UNCERTAIN;
 struct sd64_key prev={0};bool have=false;
 for(unsigned l=0;p.total&&l<=p.height;l++){
  for(uint64_t i=0;i<p.pages[l];i++){
   unsigned n=expected(&p,l,i);
   for(unsigned k=0;k<n;k++){
    uint64_t index=i*SD64_FANOUT+k;
    if(!l){
     if(!next(ctx,index,&w->records[k]))return DC64_CANCELLED;
     struct sd64_record *r=&w->records[k];
     if(!sd64_key_valid(&r->key)||!r->target||(r->kind!=SD64_FILE&&r->kind!=SD64_DIRECTORY)||
        (have&&sd64_compare(&prev,&r->key)>=0))return DC64_INVALID;
     prev=r->key;have=true;
    }else{
     uint64_t at=location(start,&p,l-1,index);unsigned cn=0;
     enum dc64_status rc=load(s,w,at,epoch,l-1,w->child,&cn);
     if(rc!=DC64_OK||cn!=expected(&p,l-1,index)){s->valid=false;return DC64_UNCERTAIN;}
     w->records[k]=(struct sd64_record){w->child[cn-1].key,at,SD64_BRANCH};
    }
   }
   if(!sd64_encode(w->page,epoch,l,w->records,n)){s->valid=false;return DC64_UNCERTAIN;}
   uint64_t at=location(start,&p,l,i);
   for(unsigned k=0;k<8;k++)if(write_sector(s,at+k,w->page+k*512)!=DC64_OK)return DC64_UNCERTAIN;
  }
 }
 if(validate(s,w,target,epoch,&p)!=DC64_OK){s->valid=false;return DC64_UNCERTAIN;}
 header(h,s->sectors,target,epoch,count);
 if(write_sector(s,target,h)!=DC64_OK)return DC64_UNCERTAIN;
 select_bank(s,target,epoch,count,&p);return DC64_OK;
}

enum dc64_status dc64_entry_at(struct dc64_state *s,struct dc64_workspace *w,
                              uint64_t epoch,uint64_t ordinal,struct sd64_record *out){
 if(!separate(s,sizeof *s,w,sizeof *w)||!separate(s,sizeof *s,out,sizeof *out)||
    !separate(w,sizeof *w,out,sizeof *out)||!s->valid||s->active>1||
    !separate(s,sizeof *s,s->block,sizeof *s->block)||
    !separate(w,sizeof *w,s->block,sizeof *s->block)||
    !separate(out,sizeof *out,s->block,sizeof *s->block))return DC64_INVALID;
 if(epoch!=s->epoch)return DC64_STALE;
 if(!grant(s->block,s->owner,s->token,s->sectors,false)){s->valid=false;return DC64_STALE;}
 if(ordinal>=s->count)return DC64_MISSING;
 struct plan p;
 if(!plan(s->count,bank_size(s->sectors)/8,&p)){s->valid=false;return DC64_CORRUPT;}
 unsigned n=0;uint64_t index=ordinal/SD64_FANOUT;
 enum dc64_status rc=load(s,w,location(base(s->sectors,s->active),&p,0,index),epoch,0,w->records,&n);
 if(rc!=DC64_OK||n!=expected(&p,0,index)){s->valid=false;return rc==DC64_OK?DC64_CORRUPT:rc;}
 *out=w->records[ordinal%SD64_FANOUT];return DC64_OK;
}
static enum dc64_status lower_bound(struct dc64_state *s,struct dc64_workspace *w,
 uint64_t epoch,const struct sd64_key *k,uint64_t *pos,struct sd64_record *found,bool *exists){
 uint64_t lo=0,hi=s->count;
 while(lo<hi){
  uint64_t mid=lo+(hi-lo)/2;struct sd64_record r;
  enum dc64_status rc=dc64_entry_at(s,w,epoch,mid,&r);
  if(rc!=DC64_OK)return rc;
  if(sd64_compare(&r.key,k)<0)lo=mid+1;else hi=mid;
 }
 *pos=lo;*exists=false;
 if(lo<s->count){
  enum dc64_status rc=dc64_entry_at(s,w,epoch,lo,found);
  if(rc!=DC64_OK)return rc;
  *exists=sd64_compare(&found->key,k)==0;
 }return DC64_OK;
}
struct edit_source {
 struct dc64_state *s;struct dc64_workspace *w;
 uint64_t epoch,remove,insert,cached_page;unsigned cached_count;
 struct sd64_record record;
 bool remove_one,insert_one,cached;
 enum dc64_status error;
};
static bool edit_next(void *ctx,uint64_t i,struct sd64_record *out){
 struct edit_source *a=ctx;
 if(a->insert_one&&i==a->insert){*out=a->record;return true;}
 uint64_t source=i-(a->insert_one&&i>a->insert?1u:0u);
 if(a->remove_one&&source>=a->remove)++source;
 if(!a->s->valid||a->s->epoch!=a->epoch||source>=a->s->count){a->error=DC64_STALE;return false;}
 uint64_t page=source/SD64_FANOUT;
 if(!a->cached||a->cached_page!=page){
  struct sd64_record check;
  a->error=dc64_entry_at(a->s,a->w,a->epoch,source,&check);
  if(a->error!=DC64_OK)return false;
  a->cached_page=page;a->cached=true;
  uint64_t rem=a->s->count-page*SD64_FANOUT;
  a->cached_count=rem<SD64_FANOUT?(unsigned)rem:SD64_FANOUT;
 }
 unsigned slot=(unsigned)(source%SD64_FANOUT);
 if(slot>=a->cached_count){a->error=DC64_CORRUPT;return false;}
 *out=a->w->records[slot];return true;
}
enum dc64_status dc64_apply(struct dc64_state *s,struct dc64_workspace *writer,
                           struct dc64_workspace *reader,uint64_t epoch,const struct dc64_edit *edit){
 if(!separate(s,sizeof *s,writer,sizeof *writer)||!separate(s,sizeof *s,reader,sizeof *reader)||
    !separate(reader,sizeof *reader,writer,sizeof *writer)||!separate(s,sizeof *s,edit,sizeof *edit)||
    !separate(reader,sizeof *reader,edit,sizeof *edit)||!separate(writer,sizeof *writer,edit,sizeof *edit)||
    !s->valid||s->active>1||!separate(edit,sizeof *edit,s->block,sizeof *s->block)||
    !separate(reader,sizeof *reader,s->block,sizeof *s->block)||
    !separate(writer,sizeof *writer,s->block,sizeof *s->block)||
    !separate(s,sizeof *s,s->block,sizeof *s->block)||
    !sd64_key_valid(&edit->entry.key)||!edit->entry.target||
    (edit->entry.kind!=SD64_FILE&&edit->entry.kind!=SD64_DIRECTORY)||
    (edit->op!=DC64_INSERT&&edit->op!=DC64_DELETE&&edit->op!=DC64_RENAME))return DC64_INVALID;
 if(epoch!=s->epoch)return DC64_STALE;
 if(!grant(s->block,s->owner,s->token,s->sectors,true))return DC64_INVALID;
 struct edit_source a={.s=s,.w=reader,.epoch=epoch,.record=edit->entry};
 struct sd64_record old={0};bool exists=false;uint64_t old_pos=0,count=s->count;
 enum dc64_status rc=lower_bound(s,reader,epoch,&edit->entry.key,&old_pos,&old,&exists);
 if(rc!=DC64_OK)return rc;
 if(edit->op==DC64_INSERT){
  if(exists)return DC64_CONFLICT;
  if(count==UINT64_MAX)return DC64_FULL;
  a.insert_one=true;a.insert=old_pos;++count;
 }else{
  if(!exists)return DC64_MISSING;
  if(old.target!=edit->entry.target||old.kind!=edit->entry.kind)return DC64_STALE;
  a.remove_one=true;a.remove=old_pos;--count;
  if(edit->op==DC64_RENAME){
   if(!sd64_key_valid(&edit->destination))return DC64_INVALID;
   if(sd64_compare(&old.key,&edit->destination)==0)return DC64_OK;
   uint64_t pos=0;
   rc=lower_bound(s,reader,epoch,&edit->destination,&pos,&old,&exists);
   if(rc!=DC64_OK)return rc;
   if(exists)return DC64_CONFLICT;
   a.record.key=edit->destination;a.insert_one=true;
   a.insert=pos-(pos>old_pos?1u:0u);++count;
  }
 }
 rc=dc64_publish(s,writer,count,edit_next,&a);
 if(rc==DC64_CANCELLED&&a.error!=DC64_OK){s->valid=false;return a.error;}
 return rc;
}
