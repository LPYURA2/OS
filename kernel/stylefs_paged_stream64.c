#include <styleos/stylefs_paged_pair64.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static uint32_t digest(uint32_t c,const uint8_t *p,size_t n){
 for(size_t i=0;i<n;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
 return c;
}
static enum pp64_status ip(enum ip64_status r){
 switch(r){
 case IP64_OK:return PP64_OK;case IP64_FULL:return PP64_FULL;case IP64_IO:return PP64_IO;
 case IP64_UNCERTAIN:return PP64_UNCERTAIN;case IP64_CANCELLED:return PP64_CANCELLED;
 case IP64_CORRUPT:return PP64_CORRUPT;default:return PP64_INVALID;
 }
}
static enum pp64_status dc(enum dc64_status r){
 switch(r){
 case DC64_OK:return PP64_OK;case DC64_FULL:return PP64_FULL;case DC64_IO:return PP64_IO;
 case DC64_UNCERTAIN:return PP64_UNCERTAIN;case DC64_CORRUPT:return PP64_CORRUPT;
 default:return PP64_INVALID;
 }
}
static bool args(struct bs_service *b,const struct pp64_layout *l,struct pp64_stream_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,const struct sd64_key *key){
 if(!cap||!rcap||cap>SIZE_MAX/sizeof *a||rcap>SIZE_MAX/sizeof *runs)return false;
 const void *p[]={b,l,w,a,runs,key};size_t n[]={sizeof *b,sizeof *l,sizeof *w,cap*sizeof *a,rcap*sizeof *runs,sizeof *key};
 for(unsigned i=0;i<6;i++)for(unsigned j=0;j<i;j++)if(!sep(p[i],n[i],p[j],n[j]))return false;
 return sd64_key_valid(key);
}
static enum pp64_status check_key(struct bs_service *b,const struct pp64_layout *l,struct pp64_stream_workspace *w,
 const struct sd64_key *key,uint64_t id,uint64_t rev){
 uint64_t found=0;uint8_t kind=0;struct dc64_state *d=&w->pair.dir;
 enum sd64_status rc=d->count?sd64_lookup(b,l->owner,l->dir,&d->root,key,w->pair.dw.page,&found,&kind):SD64_MISSING;
 if(rc==SD64_IO)return PP64_IO;
 if(rc!=SD64_OK&&rc!=SD64_MISSING)return PP64_CORRUPT;
 if(!rev)return rc==SD64_MISSING?PP64_OK:PP64_INVALID;
 if(rc!=SD64_OK||found!=id||kind!=SD64_FILE)return PP64_INVALID;
 struct sfs64_file f;enum ip64_status ir=ip64_stat(b,l->owner,l->inode,&w->pair.inode.root,id,&w->pair.iw,&f);
 if(ir!=IP64_OK)return ip(ir);
 return f.revision==rev?PP64_OK:PP64_INVALID;
}
struct merge {
 struct bs_service *b;const struct pp64_layout *l;struct pp64_stream_workspace *w;
 const struct dc64_node *nodes;size_t count,at,manifest_len;
 struct ip64_root old;
 uint64_t id;bool added;
 enum pp64_status error;
};
static bool next(void *ctx,uint64_t ordinal,uint8_t *out,size_t cap,size_t *n){
 (void)ordinal;struct merge *m=ctx;
 while(m->at<m->count&&m->nodes[m->at].kind!=SD64_FILE)m->at++;
 uint64_t id=m->at<m->count?m->nodes[m->at].id:UINT64_MAX;
 if(!m->added&&(m->at==m->count||m->id<=id)){
  if(m->at<m->count&&id==m->id)m->at++;
  if(m->manifest_len>cap){m->error=PP64_FULL;return false;}
  *n=m->manifest_len;
  for(size_t i=0;i<*n;i++)out[i]=m->w->manifest[i];
  m->added=true;return true;
 }
 if(m->at==m->count){m->error=PP64_CORRUPT;return false;}
 size_t len=0;
 enum ip64_status rc=ip64_export(m->b,m->l->owner,m->l->inode,&m->old,id,&m->w->source,&len);
 if(rc!=IP64_OK){m->error=ip(rc);return false;}
 if(len>cap){m->error=PP64_FULL;return false;}
 for(size_t i=0;i<len;i++)out[i]=m->w->source.map[i];
 *n=len;m->at++;return true;
}
enum pp64_status pp64_put(struct bs_service *b,const struct pp64_layout *l,struct pp64_stream_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *current,struct sfs64_run *previous,size_t rcap,
 uint64_t expected,const struct sd64_key *key,uint64_t id,uint64_t rev,uint64_t bytes,
 pp64_source source,void *ctx,struct pp64_pair *out){
 if(!args(b,l,w,a,cap,current,rcap,key)||!expected||id<=1||rev==UINT64_MAX||(bytes&&!source)||
    !sep(previous,rcap*sizeof *previous,w,sizeof *w)||!sep(previous,rcap*sizeof *previous,key,sizeof *key)||
    !sep(previous,rcap*sizeof *previous,out,sizeof *out)||
    !sep(out,sizeof *out,w,sizeof *w)||!sep(out,sizeof *out,key,sizeof *key)||
    !sep(out,sizeof *out,b,sizeof *b)||!sep(out,sizeof *out,l,sizeof *l)||
    !sep(out,sizeof *out,a,cap*sizeof *a)||!sep(out,sizeof *out,current,rcap*sizeof *current))return PP64_INVALID;
 struct pp64_pair pair;
 enum pp64_status rc=pp64_mount(b,l,&w->pair,a,cap,current,rcap,&pair);
 if(rc!=PP64_OK)return rc;
 if(pair.sequence!=expected)return PP64_INVALID;
 rc=pp64_prepare(b,l,&w->pair,a,cap,current,rcap,&pair);
 if(rc!=PP64_OK)return rc;
 if(pair.sequence!=expected)return PP64_INVALID;
 if(expected==UINT64_MAX||pair.inode_epoch==UINT64_MAX||(!rev&&pair.dir_epoch==UINT64_MAX))return PP64_FULL;
 bool parent=key->parent==1,used=false;
 for(size_t i=0;i<(size_t)w->pair.dir.count;i++){
  if(a[i].id==id)used=true;
  if(a[i].id==key->parent&&a[i].kind==SD64_DIRECTORY)parent=true;
 }
 if(!parent||used!=(rev!=0))return PP64_INVALID;
 if(!rev&&w->pair.dir.count>=cap)return PP64_FULL;
 rc=check_key(b,l,w,key,id,rev);if(rc!=PP64_OK)return rc;
 size_t nr=0;
 rc=pp64_plan(b,l,&w->pair,a,cap,current,previous,rcap,expected,bytes,w->plan,PP64_FILE_RUNS,&nr);
 if(rc!=PP64_OK)return rc;
 struct sfs64_file f={id,rev+1,bytes,0};size_t len=sfs64_map_length(nr);
 if(!len||len>sizeof w->manifest||!sfs64_map_encode(w->manifest,len,f,l->data_sectors,l->data_start,w->plan,nr))return PP64_CORRUPT;
 struct sfs64_map map;
 if(!sfs64_map_decode(w->manifest,len,l->data_sectors,l->data_start,&map))return PP64_CORRUPT;
 uint32_t sum=UINT32_MAX;
 for(uint64_t at=0;at<bytes;){
  uint64_t left=bytes-at;size_t n=left>sizeof w->transfer?sizeof w->transfer:(size_t)left;
  if(!source(ctx,at,w->transfer,n))return PP64_CANCELLED;
  size_t done=0;
  while(done<n){
   struct sfs_extent64 e;uint64_t local;size_t take=0,written=0;
   if(!sfs64_map_chunk(&map,at+done,n-done,&e,&local,&take))return PP64_CORRUPT;
   if(sfs64_stage_window(b,l->owner,l->data,e,local,w->transfer+done,take,&written)!=BS_OK||written!=take)return PP64_UNCERTAIN;
   for(size_t k=0;k<take;k+=512){
    uint8_t check[512];if(bs_read_sector(b,l->owner,l->data,e.first+local/512+k/512,check)!=BS_OK)return PP64_UNCERTAIN;
    size_t used_bytes=take-k<512?take-k:512;
    for(size_t j=0;j<512;j++)if(check[j]!=(j<used_bytes?w->transfer[done+k+j]:0))return PP64_UNCERTAIN;
   }
   done+=take;
  }
  sum=digest(sum,w->transfer,n);at+=n;
 }
 f.checksum=~sum;
 if(!sfs64_map_encode(w->manifest,len,f,l->data_sectors,l->data_start,w->plan,nr))return PP64_CORRUPT;
 struct ir64_state *inode=&w->pair.inode;
 if(!rev&&inode->root.files==UINT64_MAX)return PP64_FULL;
 struct merge m={.b=b,.l=l,.w=w,.nodes=a,.count=(size_t)w->pair.dir.count,
  .manifest_len=len,.old=inode->root,.id=id,.error=PP64_OK};
 enum ip64_status ir=ir64_publish(inode,&w->pair.iw,current,rcap,inode->root.files+(rev?0:1),next,&m);
 if(ir!=IP64_OK)return ir==IP64_CANCELLED&&m.error!=PP64_OK?m.error:ip(ir);
 uint64_t de=pair.dir_epoch;
 if(!rev){
  struct dc64_edit e={.op=DC64_CREATE,.entry={*key,id,SD64_FILE}};
  enum dc64_status dr=dc64_hierarchy_apply(&w->pair.dir,&w->pair.dw,a,cap,&e);
  if(dr!=DC64_OK)return dc(dr);
  de=w->pair.dir.epoch;
 }
 return pp64_commit(b,l,&w->pair,a,cap,current,rcap,expected,de,inode->root.epoch,out);
}
enum pp64_status pp64_get(struct bs_service *b,const struct pp64_layout *l,struct pp64_stream_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,
 uint64_t expected,const struct sd64_key *key,uint64_t id,uint64_t rev,pp64_sink sink,void *ctx){
 if(!args(b,l,w,a,cap,runs,rcap,key)||!expected||id<=1||!rev||!sink)return PP64_INVALID;
 struct pp64_pair pair;
 enum pp64_status rc=pp64_mount(b,l,&w->pair,a,cap,runs,rcap,&pair);
 if(rc!=PP64_OK)return rc;
 if(pair.sequence!=expected)return PP64_INVALID;
 rc=check_key(b,l,w,key,id,rev);if(rc!=PP64_OK)return rc;
 size_t len=0;enum ip64_status ir=ip64_export(b,l->owner,l->inode,&w->pair.inode.root,id,&w->source,&len);
 if(ir!=IP64_OK)return ip(ir);
 struct sfs64_map map;
 if(!sfs64_map_decode(w->source.map,len,l->data_sectors,l->data_start,&map))return PP64_CORRUPT;
 uint32_t sum=UINT32_MAX;
 for(uint64_t at=0;at<map.file.bytes;){
  uint64_t left=map.file.bytes-at;size_t n=left>sizeof w->transfer?sizeof w->transfer:(size_t)left;
  size_t done=0;
  while(done<n){
   struct sfs_extent64 e;uint64_t local;size_t take=0,got=0;
   if(!sfs64_map_chunk(&map,at+done,n-done,&e,&local,&take))return PP64_CORRUPT;
   if(sfs64_read_window(b,l->owner,l->data,e,local,w->transfer+done,take,&got)!=BS_OK||got!=take)return PP64_IO;
   done+=take;
  }
  sum=digest(sum,w->transfer,n);
  if(!sink(ctx,at,w->transfer,n))return PP64_CANCELLED;
  at+=n;
 }
 return ~sum==map.file.checksum?PP64_OK:PP64_CORRUPT;
}
