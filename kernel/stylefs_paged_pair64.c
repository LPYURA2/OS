
#include <styleos/stylefs_paged_pair64.h>
static const uint8_t magic[8]={'S','F','6','4','P','P','G','1'};
static const uint8_t magic_ids[8]={'S','F','6','4','P','P','G','2'};
struct spans {uint64_t receipt,dir,inode,data;};
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
static bool layout(struct bs_service *b,const struct pp64_layout *l,bool write,struct spans *out){
 if(!b||b->busy||!l||!l->owner||!l->receipt||!l->dir||!l->inode||!l->data||
    l->dir_sectors<18||(l->dir_sectors-2)%16||l->inode_sectors<18||(l->inode_sectors-2)%16||
    !l->data_start||l->data_start>=l->data_sectors)return false;
 const struct bs_grant *g[4]={
 grant(b,l->owner,l->receipt,2,write),grant(b,l->owner,l->dir,l->dir_sectors,false),
 grant(b,l->owner,l->inode,l->inode_sectors,false),grant(b,l->owner,l->data,l->data_sectors,false)};
 for(unsigned i=0;i<4;i++){
  if(!g[i])return false;
  for(unsigned j=0;j<i;j++)if(g[i]->device!=g[j]->device||g[i]->generation!=g[j]->generation||overlap(g[i],g[j]))return false;
 }
 *out=(struct spans){g[0]->start,g[1]->start,g[2]->start,g[3]->start};return true;
}
static bool args(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,struct dc64_node *a,size_t cap,
 struct sfs64_run *runs,size_t rcap,struct pp64_pair *out){
 if(!cap||cap>SIZE_MAX/sizeof *a||rcap>SIZE_MAX/sizeof *runs)return false;
 const void *p[]={b,l,w,a,out,runs};size_t n[]={sizeof *b,sizeof *l,sizeof *w,cap*sizeof *a,sizeof *out,rcap*sizeof *runs};
 for(unsigned i=0;i<(rcap?6u:5u);i++)for(unsigned j=0;j<i;j++)if(!sep(p[i],n[i],p[j],n[j]))return false;
 return true;
}
static void encode(uint8_t p[512],const struct pp64_layout *l,const struct spans *s,const struct pp64_pair *v){
 for(unsigned i=0;i<512;i++)p[i]=0;
 for(unsigned i=0;i<8;i++)p[i]=v->id_floor?magic_ids[i]:magic[i];
 put(p+8,v->sequence,8);put(p+16,v->dir_epoch,8);put(p+24,v->inode_epoch,8);
 put(p+32,s->receipt,8);put(p+40,s->dir,8);put(p+48,s->inode,8);put(p+56,s->data,8);
 put(p+64,l->dir_sectors,8);put(p+72,l->inode_sectors,8);put(p+80,l->data_sectors,8);put(p+88,l->data_start,8);
 put(p+96,v->slot,4);put(p+100,crc(p,100),4);
 if(v->id_floor){put(p+104,v->id_floor,8);put(p+112,crc(p,112),4);}
}
static bool decode(const uint8_t p[512],const struct pp64_layout *l,const struct spans *s,unsigned slot,struct pp64_pair *v){
 bool ids=equal(p,magic_ids,8);
 if((!ids&&!equal(p,magic,8))||!get(p+8,8)||!get(p+16,8)||!get(p+24,8)||
 get(p+32,8)!=s->receipt||get(p+40,8)!=s->dir||get(p+48,8)!=s->inode||get(p+56,8)!=s->data||
 get(p+64,8)!=l->dir_sectors||get(p+72,8)!=l->inode_sectors||get(p+80,8)!=l->data_sectors||get(p+88,8)!=l->data_start||
 get(p+96,4)!=slot||get(p+100,4)!=crc(p,100) )return false;
 uint64_t floor=0;
 if(ids){floor=get(p+104,8);if(!floor||get(p+112,4)!=crc(p,112)||!zero(p+116,396))return false;}
 else if(!zero(p+104,408))return false;
 *v=(struct pp64_pair){get(p+8,8),get(p+16,8),get(p+24,8),slot,floor};return true;
}
static enum pp64_status select_pair(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,const struct pp64_pair *v){
 enum dc64_status dr=dc64_mount_epoch(&w->dir,b,l->owner,l->dir,l->dir_sectors,&w->dw.publish,v->dir_epoch);
 if(dr!=DC64_OK)return dr==DC64_IO?PP64_IO:dr==DC64_INVALID?PP64_INVALID:PP64_CORRUPT;
 dr=dc64_hierarchy_selected(&w->dir,&w->dw,a,cap);
 if(dr!=DC64_OK)return dr==DC64_FULL?PP64_FULL:dr==DC64_IO?PP64_IO:dr==DC64_INVALID?PP64_INVALID:PP64_CORRUPT;
 enum ip64_status ir=ir64_mount_epoch(&w->inode,b,l->owner,l->inode,l->inode_sectors,
  l->data_sectors,l->data_start,v->inode_epoch,&w->iw,runs,rcap);
 if(ir!=IP64_OK)return ir==IP64_FULL?PP64_FULL:ir==IP64_IO?PP64_IO:ir==IP64_INVALID?PP64_INVALID:PP64_CORRUPT;
 /* Unique object IDs and exact file count prove bijection. Each file
  * must resolve in the selected paged manifest root, not raw latest. */
 uint64_t files=0;
 for(size_t i=0;i<(size_t)w->dir.count;i++){
  if(v->id_floor&&a[i].id>v->id_floor)return PP64_CORRUPT;
  if(a[i].kind!=SD64_FILE)continue;
  struct sfs64_file f;
  ir=ip64_stat(b,l->owner,l->inode,&w->inode.root,a[i].id,&w->iw,&f);
  if(ir!=IP64_OK)return ir==IP64_IO?PP64_IO:ir==IP64_INVALID?PP64_INVALID:PP64_CORRUPT;
  files++;
 }
 return files==w->inode.root.files?PP64_OK:PP64_CORRUPT;
}

enum pp64_status pp64_mount(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,struct pp64_pair *out){
 struct spans s;
 if(!args(b,l,w,a,cap,runs,rcap,out)||!layout(b,l,false,&s))return PP64_INVALID;
 struct pp64_pair v[2];bool good[2],blank[2];
 for(unsigned i=0;i<2;i++){
  uint8_t p[512];
  if(bs_read_sector(b,l->owner,l->receipt,i,p)!=BS_OK)return PP64_IO;
  good[i]=decode(p,l,&s,i,&v[i]);blank[i]=zero(p,512);
 }
 if(good[0]&&good[1]&&v[0].sequence==v[1].sequence)return PP64_CORRUPT;
 unsigned first=good[1]&&(!good[0]||v[1].sequence>v[0].sequence)?1:0;
 /* ID-managed volumes never silently roll back a receipt: an older
  * watermark could recycle a previously acknowledged object ID. */
 if(good[first]&&v[first].id_floor){
  unsigned other=first^1u;
  if(good[other]){
   if(!v[other].id_floor||v[first].sequence-v[other].sequence!=1||
      v[first].id_floor<v[other].id_floor||v[first].id_floor-v[other].id_floor>1)return PP64_CORRUPT;
  }else if(!blank[other]||v[first].sequence!=1||v[first].id_floor!=1)return PP64_CORRUPT;
  enum pp64_status rc=select_pair(b,l,w,a,cap,runs,rcap,&v[first]);
  if(rc==PP64_OK)*out=v[first];
  return rc;
 }
 if(good[first^1u]&&v[first^1u].id_floor)return PP64_CORRUPT;
 for(unsigned i=0;i<2;i++){
  unsigned slot=first^i;if(!good[slot])continue;
  enum pp64_status rc=select_pair(b,l,w,a,cap,runs,rcap,&v[slot]);
  if(rc==PP64_OK){*out=v[slot];return rc;}
  if(rc!=PP64_CORRUPT)return rc;
 }
 return PP64_CORRUPT;
}
static enum pp64_status publish(struct bs_service *b,const struct pp64_layout *l,const struct spans *s,
 const struct pp64_pair *v){
 uint8_t p[512],check[512];encode(p,l,s,v);
 if(bs_write_sector(b,l->owner,l->receipt,v->slot,p)!=BS_OK||
    bs_read_sector(b,l->owner,l->receipt,v->slot,check)!=BS_OK||!equal(p,check,512))return PP64_UNCERTAIN;
 return PP64_OK;
}
enum pp64_status pp64_commit(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,uint64_t expected,uint64_t de,uint64_t fe,struct pp64_pair *out){
 struct spans s;struct pp64_pair old;
 if(!args(b,l,w,a,cap,runs,rcap,out)||!layout(b,l,true,&s)||!expected)return PP64_INVALID;
 enum pp64_status rc=pp64_mount(b,l,w,a,cap,runs,rcap,&old);
 if(rc!=PP64_OK)return rc;
 if(old.sequence!=expected||de<old.dir_epoch||fe<old.inode_epoch||
    de-old.dir_epoch>1||fe-old.inode_epoch>1||(de==old.dir_epoch&&fe==old.inode_epoch))return PP64_INVALID;
 if(old.sequence==UINT64_MAX)return PP64_FULL;
 struct dc64_state before=w->dir;
 uint64_t limit=old.id_floor;
 if(limit&&limit!=UINT64_MAX)limit++;
 struct pp64_pair v={old.sequence+1,de,fe,old.slot^1u,limit};
 rc=select_pair(b,l,w,a,cap,runs,rcap,&v);
 if(rc!=PP64_OK)return rc;
 if(old.id_floor){
  /* The new sorted graph is in a[]. Every surviving ID <= the old
   * watermark must have existed in the previous graph with same kind.
   * Only one new ID, exactly floor+1, is permitted per publication. */
  size_t retained=0,matched=0;bool added=false;
  for(size_t i=0;i<(size_t)w->dir.count;i++){
   if(a[i].id<=old.id_floor)retained++;
   else{
    if(added||old.id_floor==UINT64_MAX||a[i].id!=limit)return PP64_INVALID;
    added=true;
   }
  }
  for(uint64_t i=0;i<before.count;i++){
   struct sd64_record row;
   enum dc64_status dr=dc64_entry_at(&before,i,&w->dw,&row);
   if(dr!=DC64_OK)return dr==DC64_IO?PP64_IO:PP64_CORRUPT;
   size_t lo=0,hi=(size_t)w->dir.count;
   while(lo<hi){size_t m=lo+(hi-lo)/2;if(a[m].id<row.target)lo=m+1;else hi=m;}
   if(lo<(size_t)w->dir.count&&a[lo].id==row.target){
    if(a[lo].kind!=row.kind)return PP64_INVALID;
    matched++;
   }
  }
  if(matched!=retained)return PP64_INVALID;
  v.id_floor=added?limit:old.id_floor;
 }
 rc=publish(b,l,&s,&v);
 if(rc==PP64_OK)*out=v;
 return rc;
}
#ifdef STYLEFS_HOST_FORMAT
static enum pp64_status provision(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,struct pp64_pair *out,bool ids){
 struct spans s;if(!args(b,l,w,a,cap,runs,rcap,out)||!layout(b,l,true,&s))return PP64_INVALID;
 for(unsigned i=0;i<2;i++){
  uint8_t p[512];if(bs_read_sector(b,l->owner,l->receipt,i,p)!=BS_OK)return PP64_IO;
  if(!zero(p,512))return PP64_INVALID;
 }
 struct pp64_pair v={1,1,1,0,ids?1u:0u};
 enum pp64_status rc=select_pair(b,l,w,a,cap,runs,rcap,&v);
 if(rc!=PP64_OK)return rc;
 if(w->dir.count||w->inode.root.files)return PP64_INVALID;
 rc=publish(b,l,&s,&v);if(rc==PP64_OK)*out=v;return rc;
}
enum pp64_status pp64_provision(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,struct pp64_pair *out){
 return provision(b,l,w,a,cap,runs,rcap,out,false);
}
enum pp64_status pp64_provision_ids(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,struct pp64_pair *out){
 return provision(b,l,w,a,cap,runs,rcap,out,true);
}
#endif


static enum pp64_status from_ip(enum ip64_status rc){
 switch(rc){
 case IP64_OK:return PP64_OK;
 case IP64_FULL:return PP64_FULL;
 case IP64_IO:return PP64_IO;
 case IP64_UNCERTAIN:return PP64_UNCERTAIN;
 case IP64_CORRUPT:return PP64_CORRUPT;
 default:return PP64_INVALID;
 }
}
static enum pp64_status from_dc(enum dc64_status rc){
 switch(rc){
 case DC64_OK:return PP64_OK;
 case DC64_FULL:return PP64_FULL;
 case DC64_IO:return PP64_IO;
 case DC64_UNCERTAIN:return PP64_UNCERTAIN;
 case DC64_CORRUPT:return PP64_CORRUPT;
 default:return PP64_INVALID;
 }
}
static enum pp64_status clear(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t at){
 uint8_t p[512]={0},check[512];
 if(bs_write_sector(b,owner,token,at,p)!=BS_OK||
    bs_read_sector(b,owner,token,at,check)!=BS_OK||!zero(check,512))return PP64_UNCERTAIN;
 return PP64_OK;
}
enum pp64_status pp64_prepare(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,struct pp64_pair *out){
 struct spans s;
 if(!args(b,l,w,a,cap,runs,rcap,out)||!layout(b,l,true,&s)||
    !grant(b,l->owner,l->dir,l->dir_sectors,true)||!grant(b,l->owner,l->inode,l->inode_sectors,true)||
    !grant(b,l->owner,l->data,l->data_sectors,true))return PP64_INVALID;
 struct pp64_pair cur,other;enum pp64_status rc=pp64_mount(b,l,w,a,cap,runs,rcap,&cur);
 if(rc!=PP64_OK)return rc;
 unsigned slot=cur.slot^1u;uint8_t p[512];
 if(bs_read_sector(b,l->owner,l->receipt,slot,p)!=BS_OK)return PP64_IO;
 bool retire=false;
 if(decode(p,l,&s,slot,&other)){
  if(other.sequence>cur.sequence){
   if(other.sequence-cur.sequence!=1||other.dir_epoch<cur.dir_epoch||other.inode_epoch<cur.inode_epoch||
      other.dir_epoch-cur.dir_epoch>1||other.inode_epoch-cur.inode_epoch>1)return PP64_CORRUPT;
   retire=true;
  }else if(other.sequence==cur.sequence||cur.sequence-other.sequence!=1||
    other.dir_epoch>cur.dir_epoch||other.inode_epoch>cur.inode_epoch||
    cur.dir_epoch-other.dir_epoch>1||cur.inode_epoch-other.inode_epoch>1)return PP64_CORRUPT;
 }else if(!zero(p,512))retire=true;
 struct dc64_state d;struct ir64_state i;
 enum dc64_status dr=dc64_mount(&d,b,l->owner,l->dir,l->dir_sectors,&w->dw.publish);
 if(dr!=DC64_OK)return from_dc(dr);
 enum ip64_status ir=ir64_mount(&i,b,l->owner,l->inode,l->inode_sectors,l->data_sectors,l->data_start,&w->iw,runs,rcap);
 if(ir!=IP64_OK)return from_ip(ir);
 if(d.epoch<cur.dir_epoch||d.epoch-cur.dir_epoch>1||i.root.epoch<cur.inode_epoch||i.root.epoch-cur.inode_epoch>1||
    (d.epoch>cur.dir_epoch&&d.active==w->dir.active)||(i.root.epoch>cur.inode_epoch&&i.active==w->inode.active))return PP64_CORRUPT;
 if(retire&&(rc=clear(b,l->owner,l->receipt,slot))!=PP64_OK)return rc;
 if(d.epoch>cur.dir_epoch&&(rc=clear(b,l->owner,l->dir,d.active))!=PP64_OK)return rc;
 if(i.root.epoch>cur.inode_epoch&&(rc=clear(b,l->owner,l->inode,i.active))!=PP64_OK)return rc;
 struct pp64_pair check;rc=pp64_mount(b,l,w,a,cap,runs,rcap,&check);
 if(rc!=PP64_OK)return rc;
 if(check.sequence!=cur.sequence||check.slot!=cur.slot||check.dir_epoch!=cur.dir_epoch||check.inode_epoch!=cur.inode_epoch||check.id_floor!=cur.id_floor)return PP64_CORRUPT;
 dr=dc64_mount(&d,b,l->owner,l->dir,l->dir_sectors,&w->dw.publish);
 if(dr!=DC64_OK)return from_dc(dr);
 ir=ir64_mount(&i,b,l->owner,l->inode,l->inode_sectors,l->data_sectors,l->data_start,&w->iw,runs,rcap);
 if(ir!=IP64_OK)return from_ip(ir);
 if(d.epoch!=cur.dir_epoch||i.root.epoch!=cur.inode_epoch)return PP64_CORRUPT;
 *out=check;return PP64_OK;
}
/* Shared allocator inventory for planning and read-only space statistics.
 * Caller has validated all disjoint spans; no prepare/retire/write. */
static enum pp64_status retained_inventory(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *current,struct sfs64_run *previous,size_t rcap,
 uint64_t expected,size_t *count_current,size_t *count_previous){
 struct pp64_pair pair;
 enum pp64_status rc=pp64_mount(b,l,w,a,cap,current,rcap,&pair);
 if(rc!=PP64_OK)return rc;
 if(pair.sequence!=expected)return PP64_INVALID;
 struct ir64_state latest,prior;
 enum ip64_status ir=ir64_mount(&latest,b,l->owner,l->inode,l->inode_sectors,l->data_sectors,l->data_start,&w->iw,current,rcap);
 if(ir!=IP64_OK)return from_ip(ir);
 if(latest.root.epoch!=pair.inode_epoch)return PP64_INVALID;
 size_t nc=0,np=0;
 ir=ip64_inventory(b,l->owner,l->inode,&latest.root,&w->iw,current,rcap,&nc);
 if(ir!=IP64_OK)return from_ip(ir);
 /* Under serialized writer contract epochs are adjacent. A missing or
  * corrupt previous root is not selectable; transport/scratch errors
  * instead fail closed. A valid older root is protected even if its
  * paired directory receipt has already been retired. */
 if(pair.inode_epoch>1){
  ir=ir64_mount_epoch(&prior,b,l->owner,l->inode,l->inode_sectors,l->data_sectors,l->data_start,
   pair.inode_epoch-1,&w->iw,previous,rcap);
  if(ir==IP64_OK){
   ir=ip64_inventory(b,l->owner,l->inode,&prior.root,&w->iw,previous,rcap,&np);
   if(ir!=IP64_OK)return from_ip(ir);
  }else if(ir!=IP64_CORRUPT)return from_ip(ir);
 }
 *count_current=nc;*count_previous=np;return PP64_OK;
}
enum pp64_status pp64_plan(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *current,struct sfs64_run *previous,size_t rcap,
 uint64_t expected,uint64_t bytes,struct sfs64_run *out,size_t slots,size_t *count){
 if(!cap||cap>SIZE_MAX/sizeof *a||!rcap||rcap>SIZE_MAX/sizeof *current||slots>SIZE_MAX/sizeof *out||!expected||
    (!out&&slots))return PP64_INVALID;
 const void *p[]={b,l,w,a,current,previous,count,out};
 size_t n[]={sizeof *b,sizeof *l,sizeof *w,cap*sizeof *a,rcap*sizeof *current,rcap*sizeof *previous,sizeof *count,slots*sizeof *out};
 for(unsigned i=0;i<(slots?8u:7u);i++)for(unsigned j=0;j<i;j++)if(!sep(p[i],n[i],p[j],n[j]))return PP64_INVALID;
 size_t nc=0,np=0;
 enum pp64_status rc=retained_inventory(b,l,w,a,cap,current,previous,rcap,expected,&nc,&np);
 if(rc!=PP64_OK)return rc;
 enum sfs64_alloc_status ar=sfs64_allocate(l->data_sectors,l->data_start,
  (struct sfs64_inventory){current,nc},(struct sfs64_inventory){previous,np},bytes,out,slots,count);
 return ar==SFS64_ALLOC_OK?PP64_OK:ar==SFS64_ALLOC_FULL||ar==SFS64_ALLOC_OUTPUT?PP64_FULL:PP64_INVALID;
}

enum pp64_status pp116_space(struct bs_service *b,const struct pp64_layout *l,struct pp64_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *current,struct sfs64_run *previous,size_t rcap,
 uint64_t expected,struct sfs64_space *out){
 if(!cap||cap>SIZE_MAX/sizeof *a||!rcap||rcap>SIZE_MAX/sizeof *current||!expected)return PP64_INVALID;
 const void *p[]={b,l,w,a,current,previous,out};
 size_t n[]={sizeof *b,sizeof *l,sizeof *w,cap*sizeof *a,rcap*sizeof *current,rcap*sizeof *previous,sizeof *out};
 for(unsigned i=0;i<7;++i)for(unsigned j=0;j<i;++j)if(!sep(p[i],n[i],p[j],n[j]))return PP64_INVALID;
 size_t nc=0,np=0;
 enum pp64_status rc=retained_inventory(b,l,w,a,cap,current,previous,rcap,expected,&nc,&np);
 if(rc!=PP64_OK)return rc;
 struct sfs64_space result;
 if(sfs64_space_query(l->data_sectors,l->data_start,(struct sfs64_inventory){current,nc},
     (struct sfs64_inventory){previous,np},&result)!=SFS64_ALLOC_OK)return PP64_INVALID;
 *out=result;return PP64_OK;
}
