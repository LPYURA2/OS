#include <styleos/stylefs_widevolume64.h>
#include "stylefs_checkpoint_common_internal.h"
static const uint8_t magic[8]={'S','F','6','4','W','V','0','1'};
struct geometry {uint64_t identity,start[4],length[4],data_start;};
struct receipt {uint64_t seq,dir,inode;bool valid;};
static bool layout(struct bs_service *b,const struct wv64_layout *l,bool write,struct geometry *out){
 if(!l||l->dir_sectors<18||(l->dir_sectors-2)%16||l->inode_sectors<18||
    (l->inode_sectors-2)%16||!l->data_start||l->data_start>=l->data_sectors)return false;
 uint64_t tokens[]={l->receipt,l->dir,l->inode,l->data};
 uint64_t lengths[]={2,l->dir_sectors,l->inode_sectors,l->data_sectors};
 const struct bs_grant *g[4];
 for(unsigned i=0;i<4;i++){
  g[i]=grant(b,l->owner,tokens[i],lengths[i],write);
  if(!g[i]||(i&&(g[i]->device!=g[0]->device||g[i]->generation!=g[0]->generation)))return false;
  for(unsigned j=0;j<i;j++)if(overlap(g[i],g[j]))return false;
  out->start[i]=g[i]->start;out->length[i]=lengths[i];
 }
 out->identity=b->devices[g[0]->device].backend.identity;out->data_start=l->data_start;return true;
}
static void encode(uint8_t p[512],const struct geometry *g,struct receipt r,unsigned slot){
 for(unsigned i=0;i<512;i++)p[i]=0;
 for(unsigned i=0;i<8;i++)p[i]=magic[i];
 put(p+8,r.seq,8);put(p+16,r.dir,8);put(p+24,r.inode,8);put(p+32,g->identity,8);
 for(unsigned i=0;i<4;i++){put(p+40+i*8,g->start[i],8);put(p+72+i*8,g->length[i],8);}
 put(p+104,g->data_start,8);put(p+112,slot,4);put(p+116,crc(p,116),4);
}
static struct receipt decode(const uint8_t p[512],const struct geometry *g,unsigned slot){
 struct receipt bad={0};
 if(!equal(p,magic,8)||!get(p+8,8)||!get(p+16,8)||!get(p+24,8)||
    get(p+32,8)!=g->identity||get(p+104,8)!=g->data_start||get(p+112,4)!=slot||
    get(p+116,4)!=crc(p,116)||!zero(p+120,392))return bad;
 for(unsigned i=0;i<4;i++)if(get(p+40+i*8,8)!=g->start[i]||get(p+72+i*8,8)!=g->length[i])return bad;
 return (struct receipt){get(p+8,8),get(p+16,8),get(p+24,8),true};
}
static enum wv64_status receipts(struct bs_service *b,const struct wv64_layout *l,const struct geometry *g,
 bool strict,struct receipt x[2]){
 for(unsigned i=0;i<2;i++){
  uint8_t p[512];if(bs_read_sector(b,l->owner,l->receipt,i,p)!=BS_OK)return WV64_IO;
  x[i]=decode(p,g,i);
  if(strict&&!x[i].valid&&!zero(p,512))return WV64_CORRUPT;
 }
 if(x[0].valid&&x[1].valid){
  const struct receipt *a=x[0].seq>x[1].seq?x:x+1,*z=a==x?x+1:x;
  if(a->seq-z->seq!=1||a->dir<=z->dir||a->dir-z->dir!=1||
     a->inode<=z->inode||a->inode-z->inode!=1)return WV64_CORRUPT;
 }
 return WV64_OK;
}
static unsigned newest(const struct receipt x[2]){return x[1].valid&&(!x[0].valid||x[1].seq>x[0].seq)?1:0;}
static enum wv64_status save(struct bs_service *b,const struct wv64_layout *l,const struct geometry *g,
 struct receipt r,unsigned slot){
 uint8_t p[512],q[512];encode(p,g,r,slot);
 if(bs_write_sector(b,l->owner,l->receipt,slot,p)!=BS_OK||
    bs_read_sector(b,l->owner,l->receipt,slot,q)!=BS_OK||!equal(p,q,512))return WV64_RECOVERY;
 return WV64_OK;
}
#ifdef STYLEFS_HOST_FORMAT
enum wv64_status wv64_provision_checkpoint(struct bs_service *b,const struct wv64_layout *l,struct wv64_workspace *w){
 struct geometry g;
 if(!w||!sep(w,sizeof *w,b,sizeof *b)||!sep(w,sizeof *w,l,sizeof *l)||!layout(b,l,true,&g))return WV64_INVALID;
 struct receipt x[2];enum wv64_status rc=receipts(b,l,&g,true,x);
 if(rc!=WV64_OK)return rc;
 if(x[0].valid||x[1].valid)return WV64_INVALID;
 struct dc64_state d;struct ic64_state t;
 if(dc64_mount(&d,b,l->owner,l->dir,l->dir_sectors,&w->writer)!=DC64_OK||
    ic64_mount(&t,b,l->owner,l->inode,l->inode_sectors,l->data_sectors,l->data_start,&w->stream.commit)!=IC64_OK||
    d.count||t.table.count||d.epoch!=1||t.table.geometry.epoch!=1)return WV64_CORRUPT;
 return save(b,l,&g,(struct receipt){1,1,1,true},0);
}
#endif
struct match {const struct ns64_proof *proof;size_t at;};
static bool match_file(void *ctx,const struct sfs64_map *m){
 struct match *x=ctx;
 while(x->at<x->proof->count&&x->proof->nodes[x->at].kind==SD64_DIRECTORY)++x->at;
 if(x->at==x->proof->count||x->proof->nodes[x->at].id!=m->file.id)return false;
 ++x->at;return true;
}
static enum wv64_status validate(struct wv64_state *s,struct wv64_workspace *w){
 enum ns64_status nr=ns64_validate(&s->dir,&w->reader,s->nodes,s->slots,&s->proof);
 if(nr!=NS64_OK)return nr==NS64_IO?WV64_IO:nr==NS64_FULL?WV64_FULL:WV64_CORRUPT;
 struct match m={&s->proof,0};
 enum it64_status tr=it64_walk(&s->inode.table,w->stream.old_page,match_file,&m);
 if(tr!=IT64_OK)return tr==IT64_IO?WV64_IO:WV64_CORRUPT;
 while(m.at<s->proof.count&&s->proof.nodes[m.at].kind==SD64_DIRECTORY)++m.at;
 return m.at==s->proof.count?WV64_OK:WV64_CORRUPT;
}
static bool buffers(struct wv64_state *s,struct wv64_workspace *w,struct ow64_inventory inv[2]){
 if(!s||!w||!inv||!s->nodes||!s->slots||s->slots>SIZE_MAX/sizeof *s->nodes)return false;
 for(unsigned i=0;i<2;i++)if(!inv[i].slots||inv[i].slots>SIZE_MAX/sizeof *inv[i].entries||
    inv[i].slots>SIZE_MAX/sizeof *inv[i].runs)return false;
 const void *ptr[]={s,w,s->block,s->nodes,inv,inv[0].entries,inv[0].runs,inv[1].entries,inv[1].runs};
 const size_t sizes[]={sizeof *s,sizeof *w,sizeof *s->block,s->slots*sizeof *s->nodes,2*sizeof *inv,
 inv[0].slots*sizeof *inv[0].entries,inv[0].slots*sizeof *inv[0].runs,
 inv[1].slots*sizeof *inv[1].entries,inv[1].slots*sizeof *inv[1].runs};
 for(unsigned i=0;i<9;i++)for(unsigned j=0;j<i;j++)if(!sep(ptr[i],sizes[i],ptr[j],sizes[j]))return false;
 return true;
}
static enum wv64_status ownership(struct wv64_state *s,struct wv64_workspace *w,struct ow64_inventory inv[2]){
 struct ow64_result result;
 enum ow64_status rc=ow64_collect(&s->inode,&w->stream.commit,inv,&result);
 return rc==OW64_OK?WV64_OK:rc==OW64_IO?WV64_IO:rc==OW64_OUTPUT?WV64_FULL:WV64_CORRUPT;
}
enum wv64_status wv64_open(struct wv64_state *s,struct bs_service *b,const struct wv64_layout *l,
 bool write,struct ns64_node *nodes,size_t slots,struct wv64_workspace *w,struct ow64_inventory inv[2]){
 struct geometry g;
 if(!slots||slots>SIZE_MAX/sizeof *nodes||!layout(b,l,write,&g))return WV64_INVALID;
 const void *ptr[]={s,b,l,nodes,w,inv};
 const size_t sizes[]={sizeof *s,sizeof *b,sizeof *l,slots*sizeof *nodes,sizeof *w,2*sizeof *inv};
 for(unsigned i=0;i<6;i++)for(unsigned j=0;j<i;j++)if(!sep(ptr[i],sizes[i],ptr[j],sizes[j]))return WV64_INVALID;
 *s=(struct wv64_state){.block=b,.layout=*l,.nodes=nodes,.slots=slots,.write=write};
 if(!buffers(s,w,inv))return WV64_INVALID;
 struct receipt x[2];enum wv64_status rc=receipts(b,l,&g,write,x);
 if(rc!=WV64_OK)return rc;
 unsigned last=newest(x);
 for(unsigned k=0;k<(write?1u:2u);k++){
  const struct receipt *a=&x[last^k];if(!a->valid)continue;
  enum dc64_status dr=write?dc64_mount(&s->dir,b,l->owner,l->dir,l->dir_sectors,&w->writer):
   dc64_mount_epoch(&s->dir,b,l->owner,l->dir,l->dir_sectors,a->dir,&w->writer);
  if(dr==DC64_IO)return WV64_IO;
  if(dr!=DC64_OK){if(write)return WV64_RECOVERY;continue;}
  enum ic64_status ir=write?ic64_mount(&s->inode,b,l->owner,l->inode,l->inode_sectors,l->data_sectors,l->data_start,&w->stream.commit):
   ic64_mount_epoch(&s->inode,b,l->owner,l->inode,l->inode_sectors,l->data_sectors,l->data_start,a->inode,&w->stream.commit);
  if(ir==IC64_IO)return WV64_IO;
  if(ir!=IC64_OK){if(write)return WV64_RECOVERY;continue;}
  if(s->dir.epoch!=a->dir||s->inode.table.geometry.epoch!=a->inode)return WV64_RECOVERY;
  rc=validate(s,w);
  if(rc==WV64_IO||rc==WV64_FULL)return rc;
  if(rc!=WV64_OK){if(write)return WV64_RECOVERY;continue;}
  if(write){rc=ownership(s,w,inv);if(rc!=WV64_OK)return rc;}
  s->sequence=a->seq;s->dir_epoch=a->dir;s->inode_epoch=a->inode;s->fallback=k!=0;s->ready=true;return WV64_OK;
 }
 return WV64_CORRUPT;
}
static const struct ns64_node *find(const struct ns64_proof *p,uint64_t id){
 size_t lo=0,hi=(size_t)p->count;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;if(p->nodes[mid].id<id)lo=mid+1;else hi=mid;}
 return lo<p->count&&p->nodes[lo].id==id?p->nodes+lo:0;
}
enum wv64_status wv64_create(struct wv64_state *s,struct wv64_workspace *w,struct ow64_inventory inv[2],
 const struct sd64_record *entry,uint64_t bytes,is64_source source,void *ctx){
 if(!s||!s->ready||!s->write||s->busy)return WV64_LOCKED;
 if(!buffers(s,w,inv)||!entry||!sep(entry,sizeof *entry,s,sizeof *s)||!sep(entry,sizeof *entry,w,sizeof *w)||
    !sep(entry,sizeof *entry,s->nodes,s->slots*sizeof *s->nodes)||!sep(entry,sizeof *entry,s->block,sizeof *s->block)||
    !sep(entry,sizeof *entry,inv,2*sizeof *inv)||
    !sep(entry,sizeof *entry,inv[0].entries,inv[0].slots*sizeof *inv[0].entries)||
    !sep(entry,sizeof *entry,inv[1].entries,inv[1].slots*sizeof *inv[1].entries)||
    !sep(entry,sizeof *entry,inv[0].runs,inv[0].slots*sizeof *inv[0].runs)||
    !sep(entry,sizeof *entry,inv[1].runs,inv[1].slots*sizeof *inv[1].runs)||
    entry->kind!=SD64_FILE||entry->target<=NS64_ROOT||!sd64_key_valid(&entry->key)||(bytes&&!source))return WV64_INVALID;
 const struct ns64_node *parent=find(&s->proof,entry->key.parent);
 if(entry->key.parent!=NS64_ROOT&&(!parent||parent->kind!=SD64_DIRECTORY))return WV64_INVALID;
 if(find(&s->proof,entry->target))return WV64_CONFLICT;
 if(s->dir.count>=s->slots||s->sequence==UINT64_MAX||s->dir_epoch==UINT64_MAX||s->inode_epoch==UINT64_MAX)return WV64_FULL;
 struct geometry g;struct receipt x[2];struct wv64_layout *l=&s->layout;
 if(!layout(s->block,l,true,&g)||receipts(s->block,l,&g,true,x)!=WV64_OK){s->ready=false;return WV64_RECOVERY;}
 unsigned last=newest(x);
 if(!x[last].valid||x[last].seq!=s->sequence||x[last].dir!=s->dir_epoch||x[last].inode!=s->inode_epoch||
    s->dir.epoch!=s->dir_epoch||s->inode.table.geometry.epoch!=s->inode_epoch){s->ready=false;return WV64_RECOVERY;}
 /* Fresh directory mount detects stale raw changes before any data write. */
 struct dc64_state fresh;
 if(dc64_mount(&fresh,s->block,l->owner,l->dir,l->dir_sectors,&w->writer)!=DC64_OK||
    fresh.epoch!=s->dir_epoch||fresh.active!=s->dir.active||fresh.count!=s->dir.count){s->ready=false;return WV64_RECOVERY;}
 if(s->dir.count){
  uint64_t id;uint8_t kind;
  enum sd64_status rc=sd64_lookup(s->block,l->owner,l->dir,&s->dir.root,&entry->key,w->reader.page,&id,&kind);
  if(rc==SD64_OK)return WV64_CONFLICT;
  if(rc!=SD64_MISSING){s->ready=false;return WV64_RECOVERY;}
 }
 struct dc64_edit edit={.op=DC64_INSERT,.entry=*entry};
 s->busy=true;s->ready=false;
 enum is64_status ir=is64_create(&s->inode,l->data,&w->stream,inv,edit.entry.target,bytes,source,ctx);
 if(ir!=IS64_OK){
  s->busy=false;
  /* A cancelled stream or known full/invalid plan cannot have published a root. */
  if((ir==IS64_CANCELLED||ir==IS64_FULL||ir==IS64_INVALID)&&s->inode.table.valid&&
     s->inode.table.geometry.epoch==s->inode_epoch){
   s->ready=true;return ir==IS64_CANCELLED?WV64_CANCELLED:ir==IS64_FULL?WV64_FULL:WV64_INVALID;
  }
  return WV64_RECOVERY;
 }
 enum ns64_status nr=ns64_apply(&s->dir,&s->proof,&w->writer,&w->reader,&edit);
 if(nr!=NS64_OK||validate(s,w)!=WV64_OK||
    s->dir.epoch!=s->dir_epoch+1||s->inode.table.geometry.epoch!=s->inode_epoch+1){
  s->busy=false;return WV64_RECOVERY;
 }
 enum wv64_status rc=save(s->block,l,&g,(struct receipt){s->sequence+1,s->dir.epoch,s->inode.table.geometry.epoch,true},last^1u);
 s->busy=false;
 if(rc!=WV64_OK)return WV64_RECOVERY;
 ++s->sequence;s->dir_epoch=s->dir.epoch;s->inode_epoch=s->inode.table.geometry.epoch;s->ready=true;return WV64_OK;
}
enum wv64_status wv64_stat(struct wv64_state *s,struct wv64_workspace *w,const uint8_t *path,size_t n,struct sfs64_file *out){
 if(!s||!s->ready||s->busy)return WV64_LOCKED;
 if(!sep(w,sizeof *w,s,sizeof *s)||!sep(w,sizeof *w,s->block,sizeof *s->block)||
    !sep(w,sizeof *w,s->nodes,s->slots*sizeof *s->nodes)||!sep(out,sizeof *out,w,sizeof *w)||
    !sep(out,sizeof *out,s,sizeof *s)||!sep(out,sizeof *out,s->nodes,s->slots*sizeof *s->nodes)||
    !sep(out,sizeof *out,s->block,sizeof *s->block)||!sep(out,sizeof *out,path,n))return WV64_INVALID;
 uint64_t id;uint8_t kind;
 enum ns64_status nr=ns64_resolve(&s->dir,&s->proof,path,n,w->reader.page,&id,&kind);
 if(nr==NS64_MISSING)return WV64_MISSING;
 if(nr!=NS64_OK){if(nr==NS64_IO||nr==NS64_CORRUPT||nr==NS64_STALE)s->ready=false;return nr==NS64_IO?WV64_IO:WV64_INVALID;}
 if(kind!=SD64_FILE)return WV64_INVALID;
 struct sfs64_map m;
 enum it64_status ir=it64_lookup(&s->inode.table,s->inode_epoch,id,w->stream.old_page,&m);
 if(ir!=IT64_OK){s->ready=false;return ir==IT64_IO?WV64_IO:WV64_CORRUPT;}
 *out=m.file;return WV64_OK;
}
