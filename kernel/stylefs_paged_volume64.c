#include <styleos/stylefs_paged_volume64.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static enum po64_status result(enum bs_status rc){
 switch(rc){
 case BS_OK:return PO64_OK;case BS_BUSY:return PO64_BUSY;
 case BS_DENIED:return PO64_DENIED;case BS_FULL:return PO64_FULL;
 case BS_STALE:return PO64_STALE;case BS_IO:case BS_RECOVERY:return PO64_IO;
 case BS_UNCERTAIN:return PO64_UNCERTAIN;default:return PO64_INVALID;
 }
}
static enum po64_status release(struct pv64_volume *v){
 uint64_t *tokens[]={&v->layout.receipt,&v->layout.dir,&v->layout.inode,&v->layout.data};
 enum po64_status rc=PO64_OK;
 for(unsigned i=0;i<4;i++){
  if(!*tokens[i])continue;
  enum bs_status r=bs_revoke(v->block,v->layout.owner,*tokens[i]);
  if(r==BS_OK||r==BS_STALE)*tokens[i]=0;
  else rc=result(r);
 }
 if(rc==PO64_OK){v->attached=false;v->block=0;v->generation=0;}
 return rc;
}
enum po64_status pv64_close(struct pv64_volume *v){
 if(!v)return PO64_INVALID;
 if(v->objects.busy)return PO64_BUSY;
 if(!v->attached)return PO64_OK;
 if(v->block->busy)return PO64_BUSY;
 if(v->objects.active){
  enum po64_status r=po64_unmount(&v->objects);if(r!=PO64_OK)return r;
 }
 return release(v);
}
enum po64_status pv64_mount(struct pv64_volume *v,struct bs_service *b,uint64_t generation,uint64_t owner,
 const struct pv64_geometry *g,bool write,struct pp64_stream_workspace *w,struct dc64_node *a,size_t cap,
 struct sfs64_run *runs,struct sfs64_run *previous,size_t rcap){
 if(!v||!generation||!owner||!cap||!rcap||cap>SIZE_MAX/sizeof *a||rcap>SIZE_MAX/sizeof *runs)return PO64_INVALID;
 if(v->objects.busy)return PO64_BUSY;
 if(v->attached||v->objects.active)return PO64_BUSY;
 const void *p[]={v,b,g,w,a,runs,previous};
 size_t n[]={sizeof *v,sizeof *b,sizeof *g,sizeof *w,cap*sizeof *a,rcap*sizeof *runs,rcap*sizeof *previous};
 for(unsigned i=0;i<(write?7u:6u);i++)for(unsigned j=0;j<i;j++)if(!sep(p[i],n[i],p[j],n[j]))return PO64_INVALID;
 if(b->busy)return PO64_BUSY;
 if(g->start>UINT64_MAX-g->sectors||g->sectors<2||g->dir_sectors<18||(g->dir_sectors-2)%16||
    g->inode_sectors<18||(g->inode_sectors-2)%16||g->dir_sectors>g->sectors-2||
    g->inode_sectors>g->sectors-2-g->dir_sectors)return PO64_INVALID;
 uint64_t data=g->sectors-2-g->dir_sectors-g->inode_sectors;
 if(!g->data_start||g->data_start>=data)return PO64_INVALID;
 const struct bs_device *device=0;
 for(unsigned i=0;i<BS_DEVICES;i++)if(b->devices[i].generation==generation)device=&b->devices[i];
 if(!device)return PO64_STALE;
 if(device->recovery)return PO64_IO;
 if(g->start>=device->backend.sectors||g->sectors>device->backend.sectors-g->start)return PO64_INVALID;
 if(write&&(!device->backend.writable||device->backend.system_media))return PO64_DENIED;
 struct pp64_layout l={.owner=owner,.dir_sectors=g->dir_sectors,.inode_sectors=g->inode_sectors,.data_sectors=data,.data_start=g->data_start};
 v->block=b;v->generation=generation;v->layout=l;v->attached=true;
 uint64_t pos[]={g->start,g->start+2,g->start+2+g->dir_sectors,g->start+2+g->dir_sectors+g->inode_sectors};
 uint64_t len[]={2,g->dir_sectors,g->inode_sectors,data};
 uint64_t *tokens[]={&v->layout.receipt,&v->layout.dir,&v->layout.inode,&v->layout.data};
 for(unsigned i=0;i<4;i++){
  enum bs_status br=bs_grant_range(b,generation,owner,pos[i],len[i],write?BS_READ|BS_WRITE:BS_READ,tokens[i]);
  if(br!=BS_OK){enum po64_status rr=release(v);return rr==PO64_OK?result(br):rr;}
 }
 enum po64_status rc=write?po64_mount_rw(&v->objects,b,&v->layout,w,a,cap,runs,previous,rcap):
  po64_mount(&v->objects,b,&v->layout,w,a,cap,runs,rcap);
 if(rc!=PO64_OK){enum po64_status rr=release(v);return rr==PO64_OK?rc:rr;}
 return PO64_OK;
}
