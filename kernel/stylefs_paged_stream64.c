#include <styleos/stylefs_paged_pair64.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
/* IEEE CRC32, same polynomial and initial/final state as the bitwise
 * implementation. Read-only table avoids eight iterations per byte. */
static const uint32_t crc_table[256]={
 UINT32_C(0x00000000),UINT32_C(0x77073096),UINT32_C(0xee0e612c),UINT32_C(0x990951ba),
 UINT32_C(0x076dc419),UINT32_C(0x706af48f),UINT32_C(0xe963a535),UINT32_C(0x9e6495a3),
 UINT32_C(0x0edb8832),UINT32_C(0x79dcb8a4),UINT32_C(0xe0d5e91e),UINT32_C(0x97d2d988),
 UINT32_C(0x09b64c2b),UINT32_C(0x7eb17cbd),UINT32_C(0xe7b82d07),UINT32_C(0x90bf1d91),
 UINT32_C(0x1db71064),UINT32_C(0x6ab020f2),UINT32_C(0xf3b97148),UINT32_C(0x84be41de),
 UINT32_C(0x1adad47d),UINT32_C(0x6ddde4eb),UINT32_C(0xf4d4b551),UINT32_C(0x83d385c7),
 UINT32_C(0x136c9856),UINT32_C(0x646ba8c0),UINT32_C(0xfd62f97a),UINT32_C(0x8a65c9ec),
 UINT32_C(0x14015c4f),UINT32_C(0x63066cd9),UINT32_C(0xfa0f3d63),UINT32_C(0x8d080df5),
 UINT32_C(0x3b6e20c8),UINT32_C(0x4c69105e),UINT32_C(0xd56041e4),UINT32_C(0xa2677172),
 UINT32_C(0x3c03e4d1),UINT32_C(0x4b04d447),UINT32_C(0xd20d85fd),UINT32_C(0xa50ab56b),
 UINT32_C(0x35b5a8fa),UINT32_C(0x42b2986c),UINT32_C(0xdbbbc9d6),UINT32_C(0xacbcf940),
 UINT32_C(0x32d86ce3),UINT32_C(0x45df5c75),UINT32_C(0xdcd60dcf),UINT32_C(0xabd13d59),
 UINT32_C(0x26d930ac),UINT32_C(0x51de003a),UINT32_C(0xc8d75180),UINT32_C(0xbfd06116),
 UINT32_C(0x21b4f4b5),UINT32_C(0x56b3c423),UINT32_C(0xcfba9599),UINT32_C(0xb8bda50f),
 UINT32_C(0x2802b89e),UINT32_C(0x5f058808),UINT32_C(0xc60cd9b2),UINT32_C(0xb10be924),
 UINT32_C(0x2f6f7c87),UINT32_C(0x58684c11),UINT32_C(0xc1611dab),UINT32_C(0xb6662d3d),
 UINT32_C(0x76dc4190),UINT32_C(0x01db7106),UINT32_C(0x98d220bc),UINT32_C(0xefd5102a),
 UINT32_C(0x71b18589),UINT32_C(0x06b6b51f),UINT32_C(0x9fbfe4a5),UINT32_C(0xe8b8d433),
 UINT32_C(0x7807c9a2),UINT32_C(0x0f00f934),UINT32_C(0x9609a88e),UINT32_C(0xe10e9818),
 UINT32_C(0x7f6a0dbb),UINT32_C(0x086d3d2d),UINT32_C(0x91646c97),UINT32_C(0xe6635c01),
 UINT32_C(0x6b6b51f4),UINT32_C(0x1c6c6162),UINT32_C(0x856530d8),UINT32_C(0xf262004e),
 UINT32_C(0x6c0695ed),UINT32_C(0x1b01a57b),UINT32_C(0x8208f4c1),UINT32_C(0xf50fc457),
 UINT32_C(0x65b0d9c6),UINT32_C(0x12b7e950),UINT32_C(0x8bbeb8ea),UINT32_C(0xfcb9887c),
 UINT32_C(0x62dd1ddf),UINT32_C(0x15da2d49),UINT32_C(0x8cd37cf3),UINT32_C(0xfbd44c65),
 UINT32_C(0x4db26158),UINT32_C(0x3ab551ce),UINT32_C(0xa3bc0074),UINT32_C(0xd4bb30e2),
 UINT32_C(0x4adfa541),UINT32_C(0x3dd895d7),UINT32_C(0xa4d1c46d),UINT32_C(0xd3d6f4fb),
 UINT32_C(0x4369e96a),UINT32_C(0x346ed9fc),UINT32_C(0xad678846),UINT32_C(0xda60b8d0),
 UINT32_C(0x44042d73),UINT32_C(0x33031de5),UINT32_C(0xaa0a4c5f),UINT32_C(0xdd0d7cc9),
 UINT32_C(0x5005713c),UINT32_C(0x270241aa),UINT32_C(0xbe0b1010),UINT32_C(0xc90c2086),
 UINT32_C(0x5768b525),UINT32_C(0x206f85b3),UINT32_C(0xb966d409),UINT32_C(0xce61e49f),
 UINT32_C(0x5edef90e),UINT32_C(0x29d9c998),UINT32_C(0xb0d09822),UINT32_C(0xc7d7a8b4),
 UINT32_C(0x59b33d17),UINT32_C(0x2eb40d81),UINT32_C(0xb7bd5c3b),UINT32_C(0xc0ba6cad),
 UINT32_C(0xedb88320),UINT32_C(0x9abfb3b6),UINT32_C(0x03b6e20c),UINT32_C(0x74b1d29a),
 UINT32_C(0xead54739),UINT32_C(0x9dd277af),UINT32_C(0x04db2615),UINT32_C(0x73dc1683),
 UINT32_C(0xe3630b12),UINT32_C(0x94643b84),UINT32_C(0x0d6d6a3e),UINT32_C(0x7a6a5aa8),
 UINT32_C(0xe40ecf0b),UINT32_C(0x9309ff9d),UINT32_C(0x0a00ae27),UINT32_C(0x7d079eb1),
 UINT32_C(0xf00f9344),UINT32_C(0x8708a3d2),UINT32_C(0x1e01f268),UINT32_C(0x6906c2fe),
 UINT32_C(0xf762575d),UINT32_C(0x806567cb),UINT32_C(0x196c3671),UINT32_C(0x6e6b06e7),
 UINT32_C(0xfed41b76),UINT32_C(0x89d32be0),UINT32_C(0x10da7a5a),UINT32_C(0x67dd4acc),
 UINT32_C(0xf9b9df6f),UINT32_C(0x8ebeeff9),UINT32_C(0x17b7be43),UINT32_C(0x60b08ed5),
 UINT32_C(0xd6d6a3e8),UINT32_C(0xa1d1937e),UINT32_C(0x38d8c2c4),UINT32_C(0x4fdff252),
 UINT32_C(0xd1bb67f1),UINT32_C(0xa6bc5767),UINT32_C(0x3fb506dd),UINT32_C(0x48b2364b),
 UINT32_C(0xd80d2bda),UINT32_C(0xaf0a1b4c),UINT32_C(0x36034af6),UINT32_C(0x41047a60),
 UINT32_C(0xdf60efc3),UINT32_C(0xa867df55),UINT32_C(0x316e8eef),UINT32_C(0x4669be79),
 UINT32_C(0xcb61b38c),UINT32_C(0xbc66831a),UINT32_C(0x256fd2a0),UINT32_C(0x5268e236),
 UINT32_C(0xcc0c7795),UINT32_C(0xbb0b4703),UINT32_C(0x220216b9),UINT32_C(0x5505262f),
 UINT32_C(0xc5ba3bbe),UINT32_C(0xb2bd0b28),UINT32_C(0x2bb45a92),UINT32_C(0x5cb36a04),
 UINT32_C(0xc2d7ffa7),UINT32_C(0xb5d0cf31),UINT32_C(0x2cd99e8b),UINT32_C(0x5bdeae1d),
 UINT32_C(0x9b64c2b0),UINT32_C(0xec63f226),UINT32_C(0x756aa39c),UINT32_C(0x026d930a),
 UINT32_C(0x9c0906a9),UINT32_C(0xeb0e363f),UINT32_C(0x72076785),UINT32_C(0x05005713),
 UINT32_C(0x95bf4a82),UINT32_C(0xe2b87a14),UINT32_C(0x7bb12bae),UINT32_C(0x0cb61b38),
 UINT32_C(0x92d28e9b),UINT32_C(0xe5d5be0d),UINT32_C(0x7cdcefb7),UINT32_C(0x0bdbdf21),
 UINT32_C(0x86d3d2d4),UINT32_C(0xf1d4e242),UINT32_C(0x68ddb3f8),UINT32_C(0x1fda836e),
 UINT32_C(0x81be16cd),UINT32_C(0xf6b9265b),UINT32_C(0x6fb077e1),UINT32_C(0x18b74777),
 UINT32_C(0x88085ae6),UINT32_C(0xff0f6a70),UINT32_C(0x66063bca),UINT32_C(0x11010b5c),
 UINT32_C(0x8f659eff),UINT32_C(0xf862ae69),UINT32_C(0x616bffd3),UINT32_C(0x166ccf45),
 UINT32_C(0xa00ae278),UINT32_C(0xd70dd2ee),UINT32_C(0x4e048354),UINT32_C(0x3903b3c2),
 UINT32_C(0xa7672661),UINT32_C(0xd06016f7),UINT32_C(0x4969474d),UINT32_C(0x3e6e77db),
 UINT32_C(0xaed16a4a),UINT32_C(0xd9d65adc),UINT32_C(0x40df0b66),UINT32_C(0x37d83bf0),
 UINT32_C(0xa9bcae53),UINT32_C(0xdebb9ec5),UINT32_C(0x47b2cf7f),UINT32_C(0x30b5ffe9),
 UINT32_C(0xbdbdf21c),UINT32_C(0xcabac28a),UINT32_C(0x53b39330),UINT32_C(0x24b4a3a6),
 UINT32_C(0xbad03605),UINT32_C(0xcdd70693),UINT32_C(0x54de5729),UINT32_C(0x23d967bf),
 UINT32_C(0xb3667a2e),UINT32_C(0xc4614ab8),UINT32_C(0x5d681b02),UINT32_C(0x2a6f2b94),
 UINT32_C(0xb40bbe37),UINT32_C(0xc30c8ea1),UINT32_C(0x5a05df1b),UINT32_C(0x2d02ef8d),
};
static uint32_t digest(uint32_t c,const uint8_t *p,size_t n){
 for(size_t i=0;i<n;i++)c=(c>>8)^crc_table[(c^p[i])&255u];
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
 if(!rev&&pair.id_floor){
  if(pair.id_floor==UINT64_MAX)return PP64_FULL;
  if(id!=pair.id_floor+1)return PP64_INVALID;
 }
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

static void pp96_wipe(void *p,size_t n){
 volatile uint8_t *v=p;while(n--)*v++=0;
}
enum pp64_status pp96_begin(struct pp96_reader *r,struct bs_service *b,const struct pp64_layout *l,
 struct pp64_stream_workspace *w,struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,
 uint64_t expected,const struct sd64_key *key,uint64_t id,uint64_t rev){
 if(!r||r->active||r->busy||!args(b,l,w,a,cap,runs,rcap,key)||!expected||id<=1||!rev||
    !sep(r,sizeof *r,b,sizeof *b)||!sep(r,sizeof *r,l,sizeof *l)||!sep(r,sizeof *r,w,sizeof *w)||
    !sep(r,sizeof *r,a,cap*sizeof *a)||!sep(r,sizeof *r,runs,rcap*sizeof *runs)||
    !sep(r,sizeof *r,key,sizeof *key))return PP64_INVALID;
 struct pp64_pair pair;
 enum pp64_status rc=pp64_mount(b,l,&w->pair,a,cap,runs,rcap,&pair);
 if(rc!=PP64_OK)return rc;
 if(pair.sequence!=expected)return PP64_INVALID;
 rc=check_key(b,l,w,key,id,rev);if(rc!=PP64_OK)return rc;
 size_t len=0;
 enum ip64_status ir=ip64_export(b,l->owner,l->inode,&w->pair.inode.root,id,&w->source,&len);
 if(ir!=IP64_OK)return ip(ir);
 struct sfs64_map map;
 if(len>sizeof r->manifest||!sfs64_map_decode(w->source.map,len,l->data_sectors,l->data_start,&map)||
    map.file.id!=id||map.file.revision!=rev)return PP64_CORRUPT;
 pp96_wipe(r,sizeof *r);r->block=b;r->layout=*l;r->map_bytes=len;r->crc=UINT32_MAX;
 for(size_t i=0;i<len;++i)r->manifest[i]=w->source.map[i];
 r->active=true;return PP64_OK;
}
enum pp64_status pp96_cancel(struct pp96_reader *r){
 if(!r||!r->active||r->busy)return PP64_INVALID;
 pp96_wipe(r,sizeof *r);return PP64_CANCELLED;
}
enum pp64_status pp96_step(struct pp96_reader *r,pp64_sink sink,void *ctx,bool *done){
 if(!r||!r->active||r->busy||!sink||!sep(done,sizeof *done,r,sizeof *r)||
    !sep(done,sizeof *done,r->block,sizeof *r->block))return PP64_INVALID;
 r->busy=true;
 struct sfs64_map map;
 enum pp64_status rc=PP64_OK;
 uint8_t part[BS_SECTOR_BYTES];
 if(r->map_bytes>sizeof r->manifest||
    !sfs64_map_decode(r->manifest,r->map_bytes,r->layout.data_sectors,r->layout.data_start,&map)||
    r->offset>map.file.bytes){rc=PP64_CORRUPT;goto finish;}
 for(unsigned budget=0;budget<PP96_STEP_SECTORS&&r->offset<map.file.bytes;++budget){
  uint64_t left=map.file.bytes-r->offset,local=0;
  size_t wanted=left>sizeof part?sizeof part:(size_t)left,take=0,got=0;
  struct sfs_extent64 extent;
  if(!sfs64_map_chunk(&map,r->offset,wanted,&extent,&local,&take)||local%BS_SECTOR_BYTES){
   rc=PP64_CORRUPT;goto finish;
  }
  if(sfs64_read_window(r->block,r->layout.owner,r->layout.data,extent,local,part,take,&got)!=BS_OK||got!=take){
   rc=PP64_IO;goto finish;
  }
  r->crc=digest(r->crc,part,take);
  if(!sink(ctx,r->offset,part,take)){rc=PP64_CANCELLED;goto finish;}
  r->offset+=take;
 }
 if(r->offset<map.file.bytes){
  pp96_wipe(part,sizeof part);r->busy=false;*done=false;return PP64_OK;
 }
 if(~r->crc!=map.file.checksum)rc=PP64_CORRUPT;
finish:
 pp96_wipe(part,sizeof part);pp96_wipe(r,sizeof *r);
 if(rc==PP64_OK)*done=true;
 return rc;
}

struct omit {
 struct bs_service *b;const struct pp64_layout *l;struct pp64_stream_workspace *w;
 const struct dc64_node *nodes;size_t count,at;uint64_t id;
 struct ip64_root old;enum pp64_status error;
};
static bool without(void *ctx,uint64_t ordinal,uint8_t *out,size_t cap,size_t *n){
 (void)ordinal;struct omit *m=ctx;
 while(m->at<m->count&&(m->nodes[m->at].kind!=SD64_FILE||m->nodes[m->at].id==m->id))m->at++;
 if(m->at==m->count){m->error=PP64_CORRUPT;return false;}
 uint64_t id=m->nodes[m->at++].id;size_t len=0;
 enum ip64_status rc=ip64_export(m->b,m->l->owner,m->l->inode,&m->old,id,&m->w->source,&len);
 if(rc!=IP64_OK){m->error=ip(rc);return false;}
 if(len>cap){m->error=PP64_FULL;return false;}
 for(size_t i=0;i<len;i++)out[i]=m->w->source.map[i];
 *n=len;return true;
}
enum pp64_status pp64_edit(struct bs_service *b,const struct pp64_layout *l,struct pp64_stream_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap,
 uint64_t expected,const struct dc64_edit *e,uint64_t rev,struct pp64_pair *out){
 if(!cap||!rcap||cap>SIZE_MAX/sizeof *a||rcap>SIZE_MAX/sizeof *runs||!expected)return PP64_INVALID;
 const void *p[]={b,l,w,a,runs,e,out};
 size_t n[]={sizeof *b,sizeof *l,sizeof *w,cap*sizeof *a,rcap*sizeof *runs,sizeof *e,sizeof *out};
 for(unsigned i=0;i<7;i++)for(unsigned j=0;j<i;j++)if(!sep(p[i],n[i],p[j],n[j]))return PP64_INVALID;
 if(!sd64_key_valid(&e->entry.key)||e->entry.target<=1||
    (e->entry.kind!=SD64_FILE&&e->entry.kind!=SD64_DIRECTORY)||
    (e->op!=DC64_CREATE&&e->op!=DC64_REMOVE&&e->op!=DC64_RENAME)||
    (e->op==DC64_CREATE&&e->entry.kind!=SD64_DIRECTORY)||
    (e->entry.kind==SD64_FILE?!rev:rev!=0)||
    (e->op==DC64_RENAME&&!sd64_key_valid(&e->destination)))return PP64_INVALID;
 struct pp64_pair cur;enum pp64_status rc=pp64_mount(b,l,&w->pair,a,cap,runs,rcap,&cur);
 if(rc!=PP64_OK)return rc;
 if(cur.sequence!=expected)return PP64_INVALID;
 rc=pp64_prepare(b,l,&w->pair,a,cap,runs,rcap,&cur);
 if(rc!=PP64_OK)return rc;
 if(cur.sequence!=expected)return PP64_INVALID;
 if(e->op==DC64_CREATE&&cur.id_floor){
  if(cur.id_floor==UINT64_MAX)return PP64_FULL;
  if(e->entry.target!=cur.id_floor+1)return PP64_INVALID;
 }
 bool deleting=e->op==DC64_REMOVE&&e->entry.kind==SD64_FILE;
 if(expected==UINT64_MAX||cur.dir_epoch==UINT64_MAX||(deleting&&cur.inode_epoch==UINT64_MAX))return PP64_FULL;
 if(e->entry.kind==SD64_FILE){
  rc=check_key(b,l,w,&e->entry.key,e->entry.target,rev);
  if(rc!=PP64_OK)return rc;
 }
 /* Delete a file map from the old, sorted validated node inventory.
  * The old directory and inode remain selected until the paired commit.
  * File remove cannot introduce hierarchy cycles or orphan children. */
 uint64_t ie=cur.inode_epoch;
 if(deleting){
  struct ir64_state *inode=&w->pair.inode;
  if(!inode->root.files)return PP64_CORRUPT;
  struct omit m={.b=b,.l=l,.w=w,.nodes=a,.count=(size_t)w->pair.dir.count,
   .id=e->entry.target,.old=inode->root,.error=PP64_OK};
  enum ip64_status ir=ir64_publish(inode,&w->pair.iw,runs,rcap,inode->root.files-1,without,&m);
  if(ir!=IP64_OK)return ir==IP64_CANCELLED&&m.error!=PP64_OK?m.error:ip(ir);
  ie=inode->root.epoch;
 }
 enum dc64_status dr=dc64_hierarchy_apply(&w->pair.dir,&w->pair.dw,a,cap,e);
 if(dr!=DC64_OK)return dc(dr);
 return pp64_commit(b,l,&w->pair,a,cap,runs,rcap,expected,w->pair.dir.epoch,ie,out);
}

struct copy48 {
 struct bs_service *b;const struct pp64_layout *l;
 struct sfs64_map map;uint64_t seen;uint32_t sum;enum pp64_status error;
};
static bool copy48_source(void *ctx,uint64_t at,uint8_t *p,size_t n){
 struct copy48 *c=ctx;
 if(at!=c->seen||at>c->map.file.bytes||n>c->map.file.bytes-at){
  c->error=PP64_CORRUPT;return false;
 }
 size_t done=0;
 while(done<n){
  struct sfs_extent64 e;uint64_t local;size_t take=0,got=0;
  if(!sfs64_map_chunk(&c->map,at+done,n-done,&e,&local,&take)){
   c->error=PP64_CORRUPT;return false;
  }
  if(sfs64_read_window(c->b,c->l->owner,c->l->data,e,local,p+done,take,&got)!=BS_OK||got!=take){
   c->error=PP64_IO;return false;
  }
  done+=take;
 }
 c->sum=digest(c->sum,p,n);c->seen+=n;
 if(c->seen==c->map.file.bytes&&~c->sum!=c->map.file.checksum){
  c->error=PP64_CORRUPT;return false;
 }
 return true;
}
enum pp64_status pp64_copy(struct bs_service *b,const struct pp64_layout *l,struct pp64_stream_workspace *w,
 struct dc64_node *a,size_t cap,struct sfs64_run *current,struct sfs64_run *previous,size_t rcap,
 uint64_t expected,const struct sd64_key *src,uint64_t id,uint64_t rev,uint64_t bytes,
 const struct sd64_key *dst,uint64_t new_id,struct pp64_pair *out){
 if(!cap||!rcap||cap>SIZE_MAX/sizeof *a||rcap>SIZE_MAX/sizeof *current||
    !expected||id<=1||new_id<=1||new_id==id||!rev)return PP64_INVALID;
 const void *p[]={b,l,w,a,current,previous,src,dst,out};
 size_t n[]={sizeof *b,sizeof *l,sizeof *w,cap*sizeof *a,rcap*sizeof *current,
  rcap*sizeof *previous,sizeof *src,sizeof *dst,sizeof *out};
 for(unsigned i=0;i<9;i++)for(unsigned j=0;j<i;j++)if(!sep(p[i],n[i],p[j],n[j]))return PP64_INVALID;
 if(!sd64_key_valid(src)||!sd64_key_valid(dst)||!sd64_compare(src,dst))return PP64_INVALID;
 struct pp64_pair pair;
 enum pp64_status rc=pp64_mount(b,l,&w->pair,a,cap,current,rcap,&pair);
 if(rc!=PP64_OK)return rc;
 if(pair.sequence!=expected)return PP64_INVALID;
 rc=check_key(b,l,w,src,id,rev);if(rc!=PP64_OK)return rc;
 rc=check_key(b,l,w,dst,new_id,0);if(rc!=PP64_OK)return rc;
 size_t len=0;
 enum ip64_status ir=ip64_export(b,l->owner,l->inode,&w->pair.inode.root,id,&w->source,&len);
 if(ir!=IP64_OK)return ip(ir);
 if(len>sizeof w->copy_map)return PP64_CORRUPT;
 for(size_t i=0;i<len;i++)w->copy_map[i]=w->source.map[i];
 struct copy48 c={.b=b,.l=l,.sum=UINT32_MAX,.error=PP64_OK};
 if(!sfs64_map_decode(w->copy_map,len,l->data_sectors,l->data_start,&c.map)||
    c.map.file.id!=id||c.map.file.revision!=rev)return PP64_CORRUPT;
 if(c.map.file.bytes!=bytes)return PP64_INVALID;
 if(!bytes&&c.map.file.checksum!=0)return PP64_CORRUPT;
 /* pp64_put protects the current and previous source extents, verifies
  * every destination sector and only then publishes a new paired root. */
 rc=pp64_put(b,l,w,a,cap,current,previous,rcap,expected,dst,new_id,0,bytes,
  copy48_source,&c,out);
 return rc==PP64_CANCELLED&&c.error!=PP64_OK?c.error:rc;
}
