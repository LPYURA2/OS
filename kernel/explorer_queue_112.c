#include <styleos/explorer_queue_112.h>
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)p[i]=(uint8_t)(v>>(i*8));}
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
}
void ex112_discard(struct ex112_queue *q){if(q){clear(q);ex111_close(&q->scope);q->volume=0;q->volume_ro_scratch=0;}}
enum po64_status ex117_bind_volume(struct ex112_queue *q,struct pv64_volume *v,struct sfs64_run *scratch){
 if(!q||!v||!q->scope.active||q->scope.fs!=&v->objects||q->state||q->volume||
    !v->attached||!v->objects.active||(v->objects.writable?!!scratch:!scratch))return PO64_INVALID;
 q->volume=v;q->volume_ro_scratch=scratch;return PO64_OK;
}
enum po64_status ex112_submit(struct ex112_queue *q,uint32_t p,uint64_t h,uint64_t r,unsigned op,uint64_t ordinal,uint64_t *id){
 if(!q||op<1||op>4||(op==3&&ordinal)||!id)return PO64_INVALID;
 if(!authorized(q,p,h,r))return PO64_DENIED;
 if(op==4&&!q->volume)return PO64_DENIED;
 if(!external(q,id,sizeof *id))return PO64_INVALID;
 if(q->state)return PO64_BUSY;
 if(q->serial==UINT64_MAX)return PO64_FULL;
 q->id=++q->serial;q->op=op;q->ordinal=ordinal;q->state=1;*id=q->id;
 return PO64_OK;
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
 q->result=rc;q->state=2;return true;
}
enum po64_status ex112_take(struct ex112_queue *q,uint32_t p,uint64_t h,uint64_t r,uint64_t id,uint8_t *out,size_t n){
 if(!q||!id)return PO64_INVALID;
 if(!authorized(q,p,h,r)||q->id!=id)return PO64_DENIED;
 size_t expected=q->op==1?EX112_WIRE_BYTES:q->op==4?EX117_VOLUME_BYTES:0;
 if(expected?(!out||n!=expected||!external(q,out,n)):(out||n))return PO64_INVALID;
 if(q->state!=2)return PO64_BUSY;
 enum po64_status rc=q->result;
 struct po64_service *fs=q->scope.fs;
 if(!fs->active||fs->mount_id!=q->scope.snapshot.mount||fs->pair.sequence!=q->scope.snapshot.sequence)rc=PO64_STALE;
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
