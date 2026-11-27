#include <styleos/stylefs_volumemount64.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static enum wv64_status status(enum bs_status rc){
 return rc==BS_FULL?WV64_FULL:rc==BS_BUSY?WV64_LOCKED:rc==BS_RECOVERY?WV64_RECOVERY:
 rc==BS_IO?WV64_IO:WV64_INVALID;
}
static enum wv64_status release(struct vm64_mount *m){
 uint64_t *tokens[]={&m->grants.receipt,&m->grants.dir,&m->grants.inode,&m->grants.data};
 m->volume.ready=false;m->volume.proof.valid=false;
 m->volume.dir.valid=false;m->volume.inode.table.valid=false;
 for(unsigned i=0;i<4;i++){
  if(!*tokens[i])continue;
  enum bs_status rc=bs_revoke(m->block,m->grants.owner,*tokens[i]);
  if(rc!=BS_OK&&rc!=BS_STALE)return status(rc);
  *tokens[i]=0;
 }
 *m=(struct vm64_mount){0};return WV64_OK;
}
enum wv64_status vm64_close(struct vm64_mount *m){
 if(!m||!m->active||!m->block)return WV64_INVALID;
 if(m->block->busy||m->volume.busy)return WV64_LOCKED;
 return release(m);
}
enum wv64_status vm64_open(struct vm64_mount *m,struct bs_service *b,const struct vm64_geometry *g,
 bool write,struct ns64_node *nodes,size_t slots,struct wv64_workspace *w,struct ow64_inventory inv[2]){
 if(!m||!b||!g||!w||!nodes||!inv||!slots||slots>SIZE_MAX/sizeof *nodes)return WV64_INVALID;
 if(m->active||b->busy)return WV64_LOCKED;
 for(unsigned i=0;i<2;i++)if(!inv[i].slots||inv[i].slots>SIZE_MAX/sizeof *inv[i].entries||
    inv[i].slots>SIZE_MAX/sizeof *inv[i].runs)return WV64_INVALID;
 const void *ptr[]={m,b,g,w,nodes,inv,inv[0].entries,inv[0].runs,inv[1].entries,inv[1].runs};
 const size_t sizes[]={sizeof *m,sizeof *b,sizeof *g,sizeof *w,slots*sizeof *nodes,2*sizeof *inv,
 inv[0].slots*sizeof *inv[0].entries,inv[0].slots*sizeof *inv[0].runs,
 inv[1].slots*sizeof *inv[1].entries,inv[1].slots*sizeof *inv[1].runs};
 for(unsigned i=0;i<10;i++)for(unsigned j=0;j<i;j++)if(!sep(ptr[i],sizes[i],ptr[j],sizes[j]))return WV64_INVALID;
 if(!g->generation||!g->owner||g->dir_sectors<18||(g->dir_sectors-2)%16||
    g->inode_sectors<18||(g->inode_sectors-2)%16||!g->data_reserved||g->data_reserved>=g->data_sectors)return WV64_INVALID;
 const struct bs_device *device=0;
 for(unsigned i=0;i<BS_DEVICES;i++)if(b->devices[i].generation==g->generation)device=b->devices+i;
 if(!device||device->backend.system_media||(write&&!device->backend.writable))return WV64_INVALID;
 if(device->recovery)return WV64_RECOVERY;
 uint64_t starts[]={g->receipt_start,g->dir_start,g->inode_start,g->data_start};
 uint64_t lengths[]={2,g->dir_sectors,g->inode_sectors,g->data_sectors};
 for(unsigned i=0;i<4;i++){
  if(starts[i]>=device->backend.sectors||lengths[i]>device->backend.sectors-starts[i])return WV64_INVALID;
  for(unsigned j=0;j<i;j++)if(starts[i]<starts[j]+lengths[j]&&starts[j]<starts[i]+lengths[i])return WV64_INVALID;
 }
 *m=(struct vm64_mount){.block=b,.active=true,.grants={.owner=g->owner,.dir_sectors=g->dir_sectors,
 .inode_sectors=g->inode_sectors,.data_sectors=g->data_sectors,.data_start=g->data_reserved}};
 uint64_t *tokens[]={&m->grants.receipt,&m->grants.dir,&m->grants.inode,&m->grants.data};
 for(unsigned i=0;i<4;i++){
  enum bs_status rc=bs_grant_range(b,g->generation,g->owner,starts[i],lengths[i],BS_READ|(write?BS_WRITE:0),tokens[i]);
  if(rc!=BS_OK){enum wv64_status clean=release(m);return clean==WV64_OK?status(rc):clean;}
 }
 enum wv64_status rc=wv64_open(&m->volume,b,&m->grants,write,nodes,slots,w,inv);
 if(rc!=WV64_OK){enum wv64_status clean=release(m);return clean==WV64_OK?rc:clean;}
 return WV64_OK;
}
