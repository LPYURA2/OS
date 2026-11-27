
#include <styleos/stylefs_paged_root64.h>
static const uint8_t magic[8]={'S','F','6','4','P','G','R','1'};
struct positions {uint64_t meta,dir,data;};
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool eq(const uint8_t *p,const uint8_t *q,size_t n){for(size_t i=0;i<n;i++)if(p[i]!=q[i])return false;return true;}
static bool zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t c=UINT32_MAX;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}return ~c;}
static const struct bs_grant *grant(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t n,bool write){
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=&b->grants[i];
  if(g->token!=token)continue;
  if(g->owner!=owner||g->length!=n||g->device>=BS_DEVICES||!(g->rights&BS_READ)||
     (write&&!(g->rights&BS_WRITE)))return 0;
  const struct bs_device *d=&b->devices[g->device];
  if(!g->generation||d->generation!=g->generation||d->recovery||
     g->start>=d->backend.sectors||n>d->backend.sectors-g->start||
     (write&&(!d->backend.writable||d->backend.system_media)))return 0;
  return g;
 }
 return 0;
}
static bool overlaps(const struct bs_grant *a,const struct bs_grant *b){
 return a->start<b->start+b->length&&b->start<a->start+a->length;
}
static bool layout(struct bs_service *b,const struct pr64_layout *l,bool write,struct positions *p){
 if(!b||!l||b->busy||!l->owner||!l->meta||!l->dir||!l->data||
    l->meta_sectors<18||(l->meta_sectors-2)%16||l->dir_sectors<18||(l->dir_sectors-2)%16||
    !l->data_start||l->data_start>=l->data_sectors)return false;
 const struct bs_grant *m=grant(b,l->owner,l->meta,l->meta_sectors,write);
 const struct bs_grant *d=grant(b,l->owner,l->dir,l->dir_sectors,false);
 const struct bs_grant *f=grant(b,l->owner,l->data,l->data_sectors,false);
 if(!m||!d||!f||m->device!=d->device||m->device!=f->device||
    m->generation!=d->generation||m->generation!=f->generation||
    overlaps(m,d)||overlaps(m,f)||overlaps(d,f))return false;
 *p=(struct positions){m->start,d->start,f->start};return true;
}
static bool args(struct bs_service *b,const struct pr64_layout *l,struct pr64_workspace *w,
 struct dc64_node *nodes,size_t nc,struct sfs64_run *runs,size_t rc,struct pr64_view *out){
 if(!nc||nc>SIZE_MAX/sizeof *nodes||rc>SIZE_MAX/sizeof *runs)return false;
 const void *p[]={b,l,w,nodes,out,runs};
 size_t n[]={sizeof *b,sizeof *l,sizeof *w,nc*sizeof *nodes,sizeof *out,rc*sizeof *runs};
 for(unsigned i=0;i<6;i++)if(n[i])for(unsigned j=0;j<i;j++)if(n[j]&&!sep(p[i],n[i],p[j],n[j]))return false;
 return true;
}
static struct ip64_root geometry(const struct pr64_layout *l,unsigned slot,uint64_t seq){
 uint64_t bank=(l->meta_sectors-2)/2;
 return (struct ip64_root){.start=2+slot*bank,.sectors=bank,.epoch=seq,
  .data_capacity=l->data_sectors,.data_start=l->data_start};
}
static void encode(uint8_t p[512],const struct pr64_layout *l,const struct positions *s,const struct pr64_view *v){
 for(unsigned i=0;i<512;i++)p[i]=0;
 for(unsigned i=0;i<8;i++)p[i]=magic[i];
 put(p+8,v->sequence,8);put(p+16,v->dir_epoch,8);
 put(p+24,v->inode.pages,8);put(p+32,v->inode.files,8);
 put(p+40,s->meta,8);put(p+48,s->dir,8);put(p+56,s->data,8);
 put(p+64,l->meta_sectors,8);put(p+72,l->dir_sectors,8);
 put(p+80,l->data_sectors,8);put(p+88,l->data_start,8);
 put(p+96,v->slot,4);put(p+100,crc(p,100),4);
}
static bool decode(const uint8_t p[512],const struct pr64_layout *l,const struct positions *s,unsigned slot,struct pr64_view *v){
 if(!eq(p,magic,8)||!get(p+8,8)||!get(p+16,8)||get(p+40,8)!=s->meta||
    get(p+48,8)!=s->dir||get(p+56,8)!=s->data||get(p+64,8)!=l->meta_sectors||
    get(p+72,8)!=l->dir_sectors||get(p+80,8)!=l->data_sectors||get(p+88,8)!=l->data_start||
    get(p+96,4)!=slot||get(p+100,4)!=crc(p,100)||!zero(p+104,408))return false;
 *v=(struct pr64_view){.sequence=get(p+8,8),.dir_epoch=get(p+16,8),.slot=slot};
 v->inode=geometry(l,slot,v->sequence);v->inode.pages=get(p+24,8);v->inode.files=get(p+32,8);
 return v->inode.pages<=v->inode.sectors/8&&v->inode.pages<=v->inode.files&&
    (!!v->inode.pages==!!v->inode.files);
}
static enum pr64_status from_ip(enum ip64_status x){
 switch(x){
 case IP64_OK:return PR64_OK;case IP64_IO:return PR64_IO;case IP64_FULL:return PR64_FULL;
 case IP64_CANCELLED:return PR64_CANCELLED;case IP64_UNCERTAIN:return PR64_UNCERTAIN;
 case IP64_CORRUPT:case IP64_MISSING:return PR64_CORRUPT;default:return PR64_INVALID;
 }
}
static enum pr64_status from_dc(enum dc64_status x){
 switch(x){
 case DC64_OK:return PR64_OK;case DC64_IO:return PR64_IO;case DC64_FULL:return PR64_FULL;
 case DC64_CORRUPT:return PR64_CORRUPT;case DC64_UNCERTAIN:return PR64_UNCERTAIN;
 case DC64_CANCELLED:return PR64_CANCELLED;default:return PR64_INVALID;
 }
}
static enum pr64_status directory(struct bs_service *b,const struct pr64_layout *l,struct pr64_workspace *w,
 struct dc64_node *nodes,size_t nc,uint64_t epoch){
 enum dc64_status x=dc64_mount_epoch(&w->dir,b,l->owner,l->dir,l->dir_sectors,&w->dw.publish,epoch);
 if(x!=DC64_OK)return from_dc(x);
 return from_dc(dc64_hierarchy_selected(&w->dir,&w->dw,nodes,nc));
}
static enum pr64_status validate(struct bs_service *b,const struct pr64_layout *l,struct pr64_workspace *w,
 struct dc64_node *nodes,size_t nc,struct sfs64_run *runs,size_t nr,const struct pr64_view *v){
 enum pr64_status x=directory(b,l,w,nodes,nc,v->dir_epoch);
 if(x!=PR64_OK)return x;
 x=from_ip(ip64_validate(b,l->owner,l->meta,&v->inode,&w->iw,runs,nr));
 if(x!=PR64_OK)return x;
 uint64_t files=0;
 for(size_t i=0;i<(size_t)w->dir.count;i++)if(nodes[i].kind==SD64_FILE){
  struct sfs64_file f;
  x=from_ip(ip64_stat(b,l->owner,l->meta,&v->inode,nodes[i].id,&w->iw,&f));
  if(x!=PR64_OK)return x;
  files++;
 }
 return files==v->inode.files?PR64_OK:PR64_CORRUPT;
}
enum pr64_status pr64_mount(struct bs_service *b,const struct pr64_layout *l,struct pr64_workspace *w,
 struct dc64_node *nodes,size_t nc,struct sfs64_run *runs,size_t nr,struct pr64_view *out){
 struct positions s;
 if(!args(b,l,w,nodes,nc,runs,nr,out)||!layout(b,l,false,&s))return PR64_INVALID;
 bool good[2];struct pr64_view v[2];uint8_t p[512];
 for(unsigned i=0;i<2;i++){
  if(bs_read_sector(b,l->owner,l->meta,i,p)!=BS_OK)return PR64_IO;
  good[i]=decode(p,l,&s,i,&v[i]);
 }
 if(good[0]&&good[1]&&v[0].sequence==v[1].sequence)return PR64_CORRUPT;
 unsigned first=good[1]&&(!good[0]||v[1].sequence>v[0].sequence)?1:0;
 for(unsigned i=0;i<2;i++){
  unsigned slot=first^i;if(!good[slot])continue;
  enum pr64_status x=validate(b,l,w,nodes,nc,runs,nr,&v[slot]);
  if(x==PR64_OK){*out=v[slot];return x;}
  if(x!=PR64_CORRUPT)return x;
 }
 return PR64_CORRUPT;
}
static enum pr64_status write_receipt(struct bs_service *b,const struct pr64_layout *l,unsigned slot,const uint8_t p[512]){
 uint8_t check[512];
 if(bs_write_sector(b,l->owner,l->meta,slot,p)!=BS_OK||
    bs_read_sector(b,l->owner,l->meta,slot,check)!=BS_OK||!eq(p,check,512))return PR64_UNCERTAIN;
 return PR64_OK;
}
enum pr64_status pr64_publish(struct bs_service *b,const struct pr64_layout *l,struct pr64_workspace *w,
 struct dc64_node *nodes,size_t nc,struct sfs64_run *runs,size_t nr,
 uint64_t expected,uint64_t de,uint64_t files,ip64_source next,void *ctx,struct pr64_view *out){
 struct positions s;struct pr64_view old;
 if(!args(b,l,w,nodes,nc,runs,nr,out)||!layout(b,l,true,&s)||!expected||(files&&!next))return PR64_INVALID;
 enum pr64_status x=pr64_mount(b,l,w,nodes,nc,runs,nr,&old);
 if(x!=PR64_OK)return x;
 if(old.sequence!=expected||de<old.dir_epoch||de-old.dir_epoch>1)return PR64_INVALID;
 if(old.sequence==UINT64_MAX)return PR64_FULL;
 x=directory(b,l,w,nodes,nc,de);if(x!=PR64_OK)return x;
 uint64_t count=0;
 for(size_t i=0;i<(size_t)w->dir.count;i++)if(nodes[i].kind==SD64_FILE)count++;
 if(count!=files)return PR64_INVALID;
 struct pr64_view v={.sequence=old.sequence+1,.dir_epoch=de,.slot=old.slot^1u};
 struct ip64_root g=geometry(l,v.slot,v.sequence);
 /* Retire the inactive receipt before reusing its page epoch. Never
  * overwrite the selected root or its inode bank. */
 uint8_t p[512]={0};
 x=write_receipt(b,l,v.slot,p);if(x!=PR64_OK)return x;
 x=from_ip(ip64_build(b,l->owner,l->meta,&g,files,next,ctx,&w->iw,&v.inode));
 if(x!=PR64_OK)return x;
 x=validate(b,l,w,nodes,nc,runs,nr,&v);if(x!=PR64_OK)return x;
 encode(p,l,&s,&v);x=write_receipt(b,l,v.slot,p);
 if(x==PR64_OK)*out=v;
 return x;
}
#ifdef STYLEFS_HOST_FORMAT
enum pr64_status pr64_provision(struct bs_service *b,const struct pr64_layout *l,struct pr64_workspace *w,
 struct dc64_node *nodes,size_t nc,struct sfs64_run *runs,size_t nr,struct pr64_view *out){
 struct positions s;
 if(!args(b,l,w,nodes,nc,runs,nr,out)||!layout(b,l,true,&s))return PR64_INVALID;
 uint8_t p[512];
 for(unsigned i=0;i<2;i++){
  if(bs_read_sector(b,l->owner,l->meta,i,p)!=BS_OK)return PR64_IO;
  if(!zero(p,512))return PR64_INVALID;
 }
 struct pr64_view v={.sequence=1,.dir_epoch=1,.slot=0};v.inode=geometry(l,0,1);
 enum pr64_status x=validate(b,l,w,nodes,nc,runs,nr,&v);
 if(x!=PR64_OK)return x;
 if(w->dir.count)return PR64_INVALID;
 encode(p,l,&s,&v);x=write_receipt(b,l,0,p);if(x==PR64_OK)*out=v;return x;
}
#endif
