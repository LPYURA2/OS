#include <styleos/explorer_queue_112.h>
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)p[i]=(uint8_t)(v>>(i*8));}
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;++i)v|=(uint64_t)p[i]<<(i*8);return v;}
static bool separate(const void *a,size_t n,const void *b,size_t m){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&n<=UINTPTR_MAX-x&&m<=UINTPTR_MAX-y&&(x+n<=y||y+m<=x);
}
static bool external(const struct ex112_queue *q,const void *p,size_t n){
 const struct ex111_scope *s=&q->scope;
 return separate(p,n,q,sizeof *q)&&separate(p,n,s->fs,sizeof *s->fs)&&
  separate(p,n,s->users,sizeof *s->users)&&separate(p,n,s->users->processes,sizeof *s->users->processes);
}
static bool authorized(struct ex112_queue *q,uint32_t p,uint64_t h,uint64_t r){
 return q&&ex111_authorized(&q->scope,p,h,r);
}
static void clear(struct ex112_queue *q){
 q->state=0;q->id=0;q->op=0;q->ordinal=0;q->result=PO64_INVALID;
 for(unsigned i=0;i<EX112_WIRE_BYTES;++i)q->wire[i]=0;
 for(unsigned i=0;i<EX120_EDIT_BYTES;++i)q->edit[i]=0;
}
void ex112_discard(struct ex112_queue *q){if(q){clear(q);ex111_close(&q->scope);q->volume=0;q->volume_ro_scratch=0;}}
enum po64_status ex117_bind_volume(struct ex112_queue *q,struct pv64_volume *v,struct sfs64_run *scratch){
 if(!q||!v||!q->scope.active||q->scope.fs!=&v->objects||q->state||q->volume||
    !v->attached||!v->objects.active||(v->objects.writable?!!scratch:!scratch))return PO64_INVALID;
 q->volume=v;q->volume_ro_scratch=scratch;return PO64_OK;
}
enum po64_status ex112_submit(struct ex112_queue *q,uint32_t p,uint64_t h,uint64_t r,unsigned op,uint64_t ordinal,uint64_t *id){
 if(!q||op<1||(op>4&&op!=6)||((op==3||op==6)&&ordinal)||!id)return PO64_INVALID;
 if(!authorized(q,p,h,r))return PO64_DENIED;
 if(op==4&&!q->volume)return PO64_DENIED;
 if(!external(q,id,sizeof *id))return PO64_INVALID;
 if(q->state)return PO64_BUSY;
 if(q->serial==UINT64_MAX)return PO64_FULL;
 q->id=++q->serial;q->op=op;q->ordinal=ordinal;q->state=1;*id=q->id;
 return PO64_OK;
}
enum po64_status ex120_submit_edit(struct ex112_queue *q,uint32_t p,uint64_t h,uint64_t r,const uint8_t *packet,size_t n,uint64_t *id){
 if(!q||!packet||n!=EX120_EDIT_BYTES||!id)return PO64_INVALID;
 if(!authorized(q,p,h,r)||!q->scope.edit_allowed)return PO64_DENIED;
 if(!external(q,packet,n)||!external(q,id,sizeof *id)||!separate(packet,n,id,sizeof *id))return PO64_INVALID;
 if(q->state)return PO64_BUSY;
 unsigned op=(unsigned)get(packet+4,4),ancestor=(unsigned)get(packet+8,4),len=(unsigned)get(packet+12,4);
 uint64_t ordinal=get(packet+40,8),object=get(packet+48,8),revision=get(packet+56,8);
 if(get(packet,4)!=1||op<1||op>5||len>SD64_NAME)return PO64_INVALID;
 if(get(packet+16,8)!=q->scope.snapshot.mount||get(packet+24,8)!=q->scope.snapshot.sequence||
    get(packet+32,8)!=q->scope.dirs[q->scope.depth-1])return PO64_STALE;
 if(op<=2?(ordinal||object||revision):object<2)return PO64_INVALID;
 if(op==EX119_MOVE_UP?ancestor>=q->scope.depth-1:ancestor!=0)return PO64_INVALID;
 if(op==EX119_DELETE?len!=0:len==0)return PO64_INVALID;
 for(unsigned i=len;i<256;++i)if(packet[64+i])return PO64_INVALID;
 if(len){
  struct sd64_key key={.parent=1,.length=(uint16_t)len};
  for(unsigned i=0;i<len;++i)key.name[i]=packet[64+i];
  if(!sd64_key_valid(&key))return PO64_INVALID;
 }
 if(q->serial==UINT64_MAX)return PO64_FULL;
 for(unsigned i=0;i<EX120_EDIT_BYTES;++i)q->edit[i]=packet[i];
 q->id=++q->serial;q->op=5;q->state=1;*id=q->id;return PO64_OK;
}
bool ex112_pump(struct ex112_queue *q){
 if(!q||q->state!=1)return false;
 struct nf85_owner o=q->scope.owner;
 struct po64_row row;
 enum po64_status rc;
 if(q->op==1)rc=ex111_row(&q->scope,o.pid,o.handle,o.root,q->ordinal,&row);
 else if(q->op==2)rc=ex111_enter(&q->scope,o.pid,o.handle,o.root,q->ordinal);
 else if(q->op==3)rc=ex111_up(&q->scope,o.pid,o.handle,o.root);
 else if(!authorized(q,o.pid,o.handle,o.root))rc=PO64_DENIED;
 else if(q->op==5){
  const uint8_t *p=q->edit;
  unsigned len=(unsigned)get(p+12,4);
  rc=ex119_edit(&q->scope,o.pid,o.handle,o.root,(unsigned)get(p+4,4),
   get(p+40,8),get(p+48,8),get(p+56,8),(unsigned)get(p+8,4),len?p+64:0,len);
 }else if(q->op==6)rc=ex119_refresh(&q->scope,o.pid,o.handle,o.root);
 else if(q->ordinal)rc=PO64_END; /* only one explicitly authorized mounted volume */
 else rc=ex117_volume_wire(q->volume,q->scope.snapshot,q->volume_ro_scratch,q->wire,EX117_VOLUME_BYTES);
 if(rc==PO64_BUSY)return false;
 if(rc==PO64_OK&&q->op==1){
  if(!sd64_key_valid(&row.entry.key)||row.entry.target<2)rc=PO64_CORRUPT;
  else{
   put(q->wire,1,4);put(q->wire+4,row.entry.kind,4);put(q->wire+8,row.entry.key.length,4);
   put(q->wire+16,row.entry.target,8);put(q->wire+24,row.bytes,8);put(q->wire+32,row.revision,8);
   for(unsigned i=0;i<row.entry.key.length;++i)q->wire[40+i]=row.entry.key.name[i];
  }
 }
 if(rc==PO64_OK&&(q->op==5||q->op==6)){
  put(q->wire,q->scope.snapshot.mount,8);put(q->wire+8,q->scope.snapshot.sequence,8);
  put(q->wire+16,q->scope.dirs[q->scope.depth-1],8);
 }
 q->result=rc;q->state=2;return true;
}
enum po64_status ex112_take(struct ex112_queue *q,uint32_t p,uint64_t h,uint64_t r,uint64_t id,uint8_t *out,size_t n){
 if(!q||!id)return PO64_INVALID;
 if(!authorized(q,p,h,r)||q->id!=id)return PO64_DENIED;
 size_t expected=q->op==1?EX112_WIRE_BYTES:q->op==4?EX117_VOLUME_BYTES:(q->op==5||q->op==6)?EX120_RECEIPT_BYTES:0;
 if(expected?(!out||n!=expected||!external(q,out,n)):(out||n))return PO64_INVALID;
 if(q->state!=2)return PO64_BUSY;
 enum po64_status rc=q->result;
 struct po64_service *fs=q->scope.fs;
 /* Edit receipt describes the completed transaction, not a fresh view. */
 if(q->op!=5&&(!fs->active||fs->mount_id!=q->scope.snapshot.mount||fs->pair.sequence!=q->scope.snapshot.sequence))rc=PO64_STALE;
 if(q->op==4&&(!q->volume||!q->volume->attached||!q->volume->block))rc=PO64_STALE;
 if(q->op==4&&q->volume&&q->volume->attached&&q->volume->block){
  bool present=false;
  for(unsigned i=0;i<BS_DEVICES;++i){
   const struct bs_device *d=&q->volume->block->devices[i];
   if(d->generation==q->volume->generation&&d->generation&&!d->recovery)present=true;
  }
  if(!present)rc=PO64_STALE;
 }
 if(rc==PO64_OK&&expected)for(size_t i=0;i<expected;++i)out[i]=q->wire[i];
 clear(q);return rc;
}
