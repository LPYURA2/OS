#include <styleos/explorer_volume_115.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&n<=UINTPTR_MAX-x&&m<=UINTPTR_MAX-y&&(x+n<=y||y+m<=x);
}
static const struct bs_grant *grant(const struct bs_service *b,uint64_t token){
 if(!token)return 0;
 for(unsigned i=0;i<BS_GRANTS;++i)if(b->grants[i].token==token)return &b->grants[i];
 return 0;
}
enum po64_status ex115_volume_info(struct pv64_volume *v,struct ex115_volume_info *out){
 if(!v||!out||!sep(v,sizeof *v,out,sizeof *out))return PO64_INVALID;
 if(!v->attached||!v->objects.active||!v->block||!v->generation)return PO64_STALE;
 struct bs_service *b=v->block;
 if(!sep(b,sizeof *b,out,sizeof *out))return PO64_INVALID;
 if(b->busy||v->objects.busy)return PO64_BUSY;
 if(v->objects.block!=b)return PO64_CORRUPT;
 const struct bs_device *d=0;
 unsigned slot=0;
 for(unsigned i=0;i<BS_DEVICES;++i)if(b->devices[i].generation==v->generation){d=&b->devices[i];slot=i;break;}
 if(!d)return PO64_STALE;
 if(d->recovery)return PO64_IO;
 const struct pp64_layout *l=&v->layout;
 uint64_t tokens[]={l->receipt,l->dir,l->inode,l->data};
 uint64_t lens[]={2,l->dir_sectors,l->inode_sectors,l->data_sectors};
 uint64_t start=0,end=0;
 unsigned rights=v->objects.writable?BS_READ|BS_WRITE:BS_READ;
 for(unsigned i=0;i<4;++i){
  const struct bs_grant *g=grant(b,tokens[i]);
  if(!g||g->generation!=v->generation||g->owner!=l->owner||g->device!=slot||g->rights!=rights)return PO64_STALE;
  if(!lens[i]||g->length!=lens[i]||g->start>=d->backend.sectors||
     g->length>d->backend.sectors-g->start)return PO64_CORRUPT;
  if(!i)start=g->start;else if(g->start!=end)return PO64_CORRUPT;
  end=g->start+g->length;
 }
 if(d->backend.sectors>UINT64_MAX/BS_SECTOR_BYTES||(end-start)>UINT64_MAX/BS_SECTOR_BYTES)return PO64_FULL;
 struct po64_snapshot snap;
 enum po64_status rc=po64_snapshot(&v->objects,&snap);
 if(rc!=PO64_OK)return rc;
 *out=(struct ex115_volume_info){.device_generation=v->generation,.mount=snap.mount,.sequence=snap.sequence,
  .start_sector=start,.volume_bytes=(end-start)*BS_SECTOR_BYTES,.device_bytes=d->backend.sectors*BS_SECTOR_BYTES,
  .writable=v->objects.writable};
 return PO64_OK;
}
enum po64_status ex115_volume_at(struct bs_service *b,struct pv64_volume *const *v,size_t n,size_t at,struct ex115_volume_info *out){
 if(!b||!out||n>EX115_VOLUME_MAX||(n&&!v)||!sep(b,sizeof *b,out,sizeof *out)||
    (n&&!sep(v,n*sizeof *v,out,sizeof *out)))return PO64_INVALID;
 for(size_t i=0;i<n;++i){
  if(!v[i]||!sep(v[i],sizeof *v[i],out,sizeof *out))return PO64_INVALID;
  if(v[i]->attached&&v[i]->block!=b)return PO64_DENIED;
  for(size_t j=0;j<i;++j)if(v[i]==v[j])return PO64_INVALID;
 }
 if(at>=n)return PO64_END;
 return ex115_volume_info(v[at],out);
}

static void wire_put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)p[i]=(uint8_t)(v>>(8*i));}
enum po64_status ex117_volume_wire(struct pv64_volume *v,struct po64_snapshot snap,
 struct sfs64_run *scratch,uint8_t *out,size_t n){
 if(!v||!out||n!=EX117_VOLUME_BYTES||!sep(v,sizeof *v,out,n))return PO64_INVALID;
 if(!v->attached||!v->objects.active||!v->block)return PO64_STALE;
 struct po64_service *s=&v->objects;
 if(!sep(v->block,sizeof *v->block,out,n)||!sep(s->work,sizeof *s->work,out,n)||
    !sep(s->nodes,s->node_cap*sizeof *s->nodes,out,n)||!sep(s->runs,s->run_cap*sizeof *s->runs,out,n)||
    (s->writable&&!sep(s->previous,s->run_cap*sizeof *s->previous,out,n))||
    (scratch&&!sep(scratch,s->run_cap*sizeof *scratch,out,n)))return PO64_INVALID;
 struct ex115_volume_info info;
 enum po64_status rc=ex115_volume_info(v,&info);
 if(rc!=PO64_OK)return rc;
 if(info.mount!=snap.mount||info.sequence!=snap.sequence)return PO64_STALE;
 struct sfs64_space space;
 rc=po116_space(s,snap,scratch,&space);
 if(rc!=PO64_OK)return rc;
 if(space.free_sectors>info.volume_bytes/BS_SECTOR_BYTES||space.largest_run>space.free_sectors)return PO64_CORRUPT;
 uint8_t p[EX117_VOLUME_BYTES]={0};
 wire_put(p,1,4);wire_put(p+4,info.writable?1:0,4);
 wire_put(p+8,info.mount,8);wire_put(p+16,info.sequence,8);
 wire_put(p+24,info.volume_bytes,8);wire_put(p+32,info.device_bytes,8);
 wire_put(p+40,space.free_sectors*BS_SECTOR_BYTES,8);wire_put(p+48,space.largest_run*BS_SECTOR_BYTES,8);
 for(unsigned i=0;i<EX117_VOLUME_BYTES;++i)out[i]=p[i];
 return PO64_OK;
}
