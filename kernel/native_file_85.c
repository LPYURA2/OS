#include <styleos/native_file_85.h>
#include <styleos/native_file_abi.h>
static uint64_t principal(uint32_t pid){return UINT64_C(0x8000000000000000)|pid;}
static void wipe(void *p,size_t n){volatile uint8_t *b=p;for(size_t i=0;i<n;++i)b[i]=0;}
static bool sep(const void *a,size_t n,const void *b,size_t m){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&n<=UINTPTR_MAX-x&&m<=UINTPTR_MAX-y&&(x+n<=y||y+m<=x);
}
static bool ready(const struct nf85_service *s){
 return s&&s->ready&&s->fs&&s->sessions&&s->sessions->ready&&s->sessions->processes;
}
static bool external_span(const struct nf85_service *s,const void *p,size_t n){
 return sep(p,n,s,sizeof *s)&&sep(p,n,s->fs,sizeof *s->fs)&&
  sep(p,n,s->sessions,sizeof *s->sessions)&&
  sep(p,n,s->sessions->processes,sizeof *s->sessions->processes);
}
static bool owner(const struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,
                  bool selected,struct nf85_owner *out){
 if(!ready(s)||!pid||!handle||!root||(root&4095u))return false;
 const struct pr_entry *p=pr_find(s->sessions->processes,pid);
 if(!p||p->type!=PR_NATIVE_USER||!pr_is_live(s->sessions->processes,pid)||
    p->scheduler_handle!=handle||p->page_root!=root)return false;
 for(unsigned i=0;i<PR_MAX_PROCESSES;++i){
  const struct usr_owner *o=&s->sessions->owners[i];
  if(o->pid!=pid||o->scheduler_handle!=handle)continue;
  const struct usr_session *u=usr_find(s->sessions,o->session_id);
  if(!u||u->state==USR_CLOSED||(selected&&(u->state!=USR_ACTIVE||
      s->sessions->selected_session!=u->id)))return false;
  *out=(struct nf85_owner){pid,u->id,handle,root};return true;
 }
 return false;
}
static bool same(struct nf85_owner a,struct nf85_owner b){
 return a.pid==b.pid&&a.session==b.session&&a.handle==b.handle&&a.root==b.root;
}
static bool token_live(const struct nf85_service *s,const struct nf85_grant *g){
 if(!g->token||!s->fs->active||s->fs->mount_id!=g->mount)return false;
 for(unsigned i=0;i<PO64_HANDLES;++i){
  const struct po64_handle *h=&s->fs->handles[i];
  if(h->token==g->token&&h->principal==principal(g->owner.pid)&&h->rights==g->rights)return true;
 }
 return false;
}
static struct nf85_grant *find(struct nf85_service *s,struct nf85_owner o,uint64_t token){
 for(unsigned i=0;i<NF85_HANDLES;++i)
  if(s->grants[i].token==token&&token&&!s->grants[i].pending_delivery&&same(s->grants[i].owner,o))return &s->grants[i];
 return 0;
}
enum po64_status nf85_start(struct nf85_service *s,struct po64_service *fs,const struct usr_runtime *u){
 if(!s||!fs||!u||s->ready||!fs->active||!u->ready||!u->processes||
    !sep(s,sizeof *s,fs,sizeof *fs)||!sep(s,sizeof *s,u,sizeof *u)||
    !sep(s,sizeof *s,u->processes,sizeof *u->processes))return PO64_INVALID;
 /* A second broker must never adopt preexisting reserved-principal grants. */
 for(unsigned i=0;i<PO64_HANDLES;++i)
  if(fs->handles[i].token&&(fs->handles[i].principal>>63))return PO64_DENIED;
 wipe(s,sizeof *s);s->fs=fs;s->sessions=u;s->ready=true;return PO64_OK;
}
size_t nf85_reconcile(struct nf85_service *s){
 if(!ready(s)||s->busy||s->fs->busy)return 0;
 size_t count=0;
 for(unsigned i=0;i<NF85_HANDLES;++i){
  struct nf85_grant *g=&s->grants[i];
  if(!g->token)continue;
  struct nf85_owner o;
  bool live=token_live(s,g);
  if(live&&owner(s,g->owner.pid,g->owner.handle,g->owner.root,false,&o)&&same(o,g->owner))continue;
  if(live&&po64_revoke(s->fs,principal(g->owner.pid),g->token)!=PO64_OK)continue;
  wipe(g,sizeof *g);++count;
 }
 return count;
}
enum po64_status nf85_grant_access(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 const struct sd64_key *key,uint64_t id,uint64_t revision,unsigned rights,uint64_t *token){
 if(!ready(s)||!key||!token||!rights||(rights&~(PO64_READ|PO64_WRITE))||!external_span(s,token,sizeof *token)||
    !external_span(s,key,sizeof *key)||!sep(key,sizeof *key,token,sizeof *token))return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 nf85_reconcile(s);
 unsigned slot=NF85_HANDLES;
 for(unsigned i=0;i<NF85_HANDLES;++i)if(!s->grants[i].token){slot=i;break;}
 if(slot==NF85_HANDLES)return PO64_FULL;
 uint64_t value=0;
 enum po64_status rc=po64_grant_access(s->fs,principal(pid),key,id,revision,rights,&value);
 if(rc!=PO64_OK)return rc;
 s->grants[slot]=(struct nf85_grant){o,value,s->fs->mount_id,rights,false};*token=value;return PO64_OK;
}
enum po64_status nf85_grant_read(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 const struct sd64_key *key,uint64_t id,uint64_t revision,uint64_t *token){
 return nf85_grant_access(s,pid,handle,root,key,id,revision,PO64_READ,token);
}
enum po64_status nf93_offer(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 const struct sd64_key *key,uint64_t id,uint64_t revision,unsigned rights){
 if(!ready(s))return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 nf85_reconcile(s);
 for(unsigned i=0;i<NF85_HANDLES;++i)
  if(s->grants[i].pending_delivery&&same(s->grants[i].owner,o))return PO64_BUSY;
 uint64_t token=0;
 enum po64_status status=nf85_grant_access(s,pid,handle,root,key,id,revision,rights,&token);
 if(status!=PO64_OK)return status;
 struct nf85_grant *g=find(s,o,token);
 if(!g)return PO64_STALE; /* serialized grant insertion invariant */
 g->pending_delivery=true;
 return PO64_OK;
}
enum po64_status nf93_receive(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,uint64_t *token){
 if(!ready(s)||!token||!external_span(s,token,sizeof *token))return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 for(unsigned i=0;i<NF85_HANDLES;++i){
  struct nf85_grant *g=&s->grants[i];
  if(!g->pending_delivery||!same(g->owner,o))continue;
  if(!token_live(s,g)){wipe(g,sizeof *g);return PO64_STALE;}
  g->pending_delivery=false;*token=g->token;return PO64_OK;
 }
 return PO64_END;
}
struct reader {uint8_t *p;size_t capacity,seen;bool overflow;};
static bool collect(void *opaque,uint64_t offset,const uint8_t *p,size_t n){
 struct reader *r=opaque;
 if(!p||offset!=r->seen||r->seen>r->capacity||n>r->capacity-r->seen){
  r->overflow=true;return false;
 }
 for(size_t i=0;i<n;++i)r->p[r->seen+i]=p[i];
 r->seen+=n;return true;
}
enum po64_status nf85_read(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,uint8_t *out,size_t capacity,size_t *bytes){
 if(!ready(s)||!token||!out||!bytes||!capacity||capacity>NF85_READ_MAX||
    !external_span(s,out,capacity)||!external_span(s,bytes,sizeof *bytes)||
    !sep(out,capacity,bytes,sizeof *bytes))return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf85_grant *g=find(s,o,token);
 if(!g||!(g->rights&PO64_READ))return PO64_DENIED;
 if(!token_live(s,g))return PO64_STALE;
 s->busy=true;wipe(s->scratch,sizeof s->scratch);
 struct reader r={s->scratch,capacity,0,false};
 enum po64_status rc=po64_read(s->fs,principal(pid),token,collect,&r);
 if(rc==PO64_OK){
  for(size_t i=0;i<r.seen;++i)out[i]=s->scratch[i];
  *bytes=r.seen;
 }else if(rc==PO64_CANCELLED&&r.overflow)rc=PO64_FULL;
 wipe(s->scratch,sizeof s->scratch);s->busy=false;return rc;
}
struct writer {const uint8_t *p;size_t bytes;};
static bool supply(void *opaque,uint64_t offset,uint8_t *out,size_t n){
 const struct writer *w=opaque;
 if(!out||offset>w->bytes||n>w->bytes-(size_t)offset)return false;
 for(size_t i=0;i<n;++i)out[i]=w->p[(size_t)offset+i];
 return true;
}
enum po64_status nf85_write(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,const uint8_t *data,size_t bytes){
 if(!ready(s)||!token||bytes>NF85_READ_MAX||(bytes&&!external_span(s,data,bytes)))return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf85_grant *g=find(s,o,token);
 if(!g||!(g->rights&PO64_WRITE))return PO64_DENIED;
 if(!token_live(s,g))return PO64_STALE;
 s->busy=true;wipe(s->scratch,sizeof s->scratch);
 for(size_t i=0;i<bytes;++i)s->scratch[i]=data[i];
 struct writer w={s->scratch,bytes};
 enum po64_status rc=po64_write(s->fs,principal(pid),token,bytes,supply,&w);
 wipe(s->scratch,sizeof s->scratch);s->busy=false;
 /* Durable publication invalidates all prior exact-revision object tokens.
  * On uncertainty po64 closes the mount; reconciliation removes old grants. */
 nf85_reconcile(s);
 return rc;
}
struct stream85 {
 struct nf85_service *service;
 struct nf85_owner owner;
 uint64_t seen,limit;
 pp64_sink sink;
 pp64_source source;
 void *context;
 bool revoked;
};
static bool stream_owner(struct stream85 *v){
 struct nf85_owner o;
 if(owner(v->service,v->owner.pid,v->owner.handle,v->owner.root,true,&o)&&same(o,v->owner))return true;
 v->revoked=true;return false;
}
static bool stream_sink(void *opaque,uint64_t offset,const uint8_t *p,size_t n){
 struct stream85 *v=opaque;
 if(!p||!n||offset!=v->seen||n>UINT64_MAX-offset)return false;
 size_t done=0;
 while(done<n){
  if(!stream_owner(v))return false;
  size_t chunk=n-done;
  if(chunk>NF85_READ_MAX)chunk=NF85_READ_MAX;
  for(size_t i=0;i<chunk;++i)v->service->scratch[i]=p[done+i];
  bool ok=v->sink(v->context,offset+done,v->service->scratch,chunk);
  wipe(v->service->scratch,sizeof v->service->scratch);
  if(!ok||!stream_owner(v))return false;
  done+=chunk;v->seen+=chunk;
 }
 return true;
}
static bool stream_source(void *opaque,uint64_t offset,uint8_t *p,size_t n){
 struct stream85 *v=opaque;
 if(!p||!n||offset!=v->seen||offset>v->limit||n>v->limit-offset)return false;
 size_t done=0;
 while(done<n){
  if(!stream_owner(v))return false;
  size_t chunk=n-done;
  if(chunk>NF85_READ_MAX)chunk=NF85_READ_MAX;
  /* Never expose the underlying StyleFS transfer workspace to callbacks. */
  wipe(v->service->scratch,sizeof v->service->scratch);
  bool ok=v->source(v->context,offset+done,v->service->scratch,chunk);
  if(ok&&stream_owner(v))for(size_t i=0;i<chunk;++i)p[done+i]=v->service->scratch[i];
  else ok=false;
  wipe(v->service->scratch,sizeof v->service->scratch);
  if(!ok)return false;
  done+=chunk;v->seen+=chunk;
 }
 return true;
}
enum po64_status nf85_stream_read(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,pp64_sink sink,void *ctx,uint64_t *completed){
 if(!ready(s)||!token||!sink||!completed||!external_span(s,completed,sizeof *completed))return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf85_grant *g=find(s,o,token);
 if(!g||!(g->rights&PO64_READ))return PO64_DENIED;
 if(!token_live(s,g))return PO64_STALE;
 s->busy=true;wipe(s->scratch,sizeof s->scratch);
 struct stream85 v={.service=s,.owner=o,.sink=sink,.context=ctx};
 enum po64_status rc=po64_read(s->fs,principal(pid),token,stream_sink,&v);
 if(rc==PO64_OK&&stream_owner(&v))*completed=v.seen;
 else if(rc==PO64_OK||(rc==PO64_CANCELLED&&v.revoked))rc=PO64_DENIED;
 wipe(s->scratch,sizeof s->scratch);s->busy=false;
 nf85_reconcile(s);
 return rc;
}
enum po64_status nf85_stream_write(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,uint64_t bytes,pp64_source source,void *ctx){
 if(!ready(s)||!token||(bytes&&!source))return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf85_grant *g=find(s,o,token);
 if(!g||!(g->rights&PO64_WRITE))return PO64_DENIED;
 if(!token_live(s,g))return PO64_STALE;
 s->busy=true;wipe(s->scratch,sizeof s->scratch);
 struct stream85 v={.service=s,.owner=o,.limit=bytes,.source=source,.context=ctx};
 enum po64_status rc=po64_write(s->fs,principal(pid),token,bytes,stream_source,&v);
 if(rc==PO64_CANCELLED&&v.revoked)rc=PO64_DENIED;
 wipe(s->scratch,sizeof s->scratch);s->busy=false;
 nf85_reconcile(s);
 return rc;
}
enum po64_status nf85_close(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,uint64_t token){
 if(!ready(s)||!token)return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf85_grant *g=find(s,o,token);
 if(!g)return PO64_DENIED;
 if(!token_live(s,g)){wipe(g,sizeof *g);return PO64_STALE;}
 enum po64_status rc=po64_revoke(s->fs,principal(pid),token);
 if(rc==PO64_OK)wipe(g,sizeof *g);
 return rc;
}

/* Deferred requests use independent bounded storage, never retain user
 * virtual addresses, and revalidate the grant when the worker executes. */
static bool qready(const struct nf89_queue *q){return q&&ready(q->files);}
static bool qspan(const struct nf89_queue *q,const void *p,size_t n){
 return sep(p,n,q,sizeof *q)&&external_span(q->files,p,n);
}
enum po64_status nf89_start(struct nf89_queue *q,struct nf85_service *s){
 if(!q||!ready(s)||q->files||!external_span(s,q,sizeof *q))return PO64_INVALID;
 wipe(q,sizeof *q);q->files=s;return PO64_OK;
}
static struct nf89_request *qfind(struct nf89_queue *q,struct nf85_owner o,uint64_t id){
 for(unsigned i=0;i<NF89_SLOTS;++i)
  if(q->slots[i].id==id&&id&&same(q->slots[i].owner,o))return &q->slots[i];
 return 0;
}
size_t nf89_reconcile(struct nf89_queue *q){
 if(!qready(q)||q->busy||q->files->busy||q->files->fs->busy)return 0;
 size_t n=0;
 for(unsigned i=0;i<NF89_SLOTS;++i){
  struct nf89_request *r=&q->slots[i];struct nf85_owner o;
  if(!r->id)continue;
  if(!owner(q->files,r->owner.pid,r->owner.handle,r->owner.root,false,&o)||!same(o,r->owner)){
   wipe(r,sizeof *r);++n;continue;
  }
  /* Revalidate the exact grant even for already completed reads.
   * Preserve completed writes (including uncertain outcome), not a
   * misleading cancellation after the durable publication boundary. */
  struct nf85_grant *g=find(q->files,o,r->token);
  bool valid=g&&g->mount==r->mount&&token_live(q->files,g)&&
      (g->rights&(r->op==NF89_READ?PO64_READ:PO64_WRITE));
  if(!valid&&(r->state==NF89_PENDING||
     (r->state==NF89_DONE&&r->op==NF89_READ&&r->status==PO64_OK))){
   wipe(r->data,sizeof r->data);r->bytes=0;r->status=PO64_STALE;r->state=NF89_DONE;++n;
  }
 }
 return n;
}
enum po64_status nf89_submit(struct nf89_queue *q,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,enum nf89_op op,const uint8_t *data,size_t bytes,uint64_t *request){
 if(!qready(q)||!token||!request||!qspan(q,request,sizeof *request)||
    bytes>NF85_READ_MAX||(op!=NF89_READ&&op!=NF89_WRITE)||
    (op==NF89_READ&&(!bytes||data))||
    (op==NF89_WRITE&&bytes&&(!qspan(q,data,bytes)||!sep(data,bytes,request,sizeof *request))))
  return PO64_INVALID;
 if(q->busy||q->files->busy||q->files->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(q->files,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf85_grant *g=find(q->files,o,token);
 if(!g||!(g->rights&(op==NF89_READ?PO64_READ:PO64_WRITE)))return PO64_DENIED;
 if(!token_live(q->files,g))return PO64_STALE;
 nf89_reconcile(q);
 unsigned slot=NF89_SLOTS;
 for(unsigned i=0;i<NF89_SLOTS;++i){
  if(q->slots[i].id&&same(q->slots[i].owner,o))return PO64_BUSY;
  if(!q->slots[i].id&&slot==NF89_SLOTS)slot=i;
 }
 if(slot==NF89_SLOTS||q->serial==UINT64_MAX)return PO64_FULL;
 struct nf89_request *r=&q->slots[slot];
 wipe(r,sizeof *r);r->owner=o;r->id=++q->serial;r->token=token;r->mount=g->mount;
 r->op=op;r->state=NF89_PENDING;r->bytes=bytes;
 if(op==NF89_WRITE)for(size_t i=0;i<bytes;++i)r->data[i]=data[i];
 *request=r->id;return PO64_OK;
}
bool nf89_pump(struct nf89_queue *q){
 if(!qready(q)||q->busy||q->files->busy||q->files->fs->busy)return false;
 nf89_reconcile(q);
 struct nf89_request *r=0;
 for(unsigned i=0;i<NF89_SLOTS;++i){
  struct nf89_request *p=&q->slots[i];
  struct nf85_owner o;
  if(p->state!=NF89_PENDING||!owner(q->files,p->owner.pid,p->owner.handle,p->owner.root,true,&o)||
     !same(o,p->owner))continue;
  if(!r||p->id<r->id)r=p;
 }
 if(!r)return false;
 q->busy=true;
 struct nf85_owner o=r->owner;size_t n=0;
 enum po64_status status=r->op==NF89_READ?
  nf85_read(q->files,o.pid,o.handle,o.root,r->token,r->data,r->bytes,&n):
  nf85_write(q->files,o.pid,o.handle,o.root,r->token,r->data,r->bytes);
 if(r->op==NF89_WRITE||status!=PO64_OK){wipe(r->data,sizeof r->data);n=0;}
 r->bytes=n;r->status=status;r->state=NF89_DONE;q->busy=false;
 return true;
}
static struct nf89_request *read97_request(struct nf89_queue *q){
 for(unsigned i=0;i<NF89_SLOTS;++i)
  if(q->read97_id&&q->slots[i].id==q->read97_id&&q->slots[i].state==NF89_PENDING&&q->slots[i].op==NF89_READ)
   return &q->slots[i];
 return 0;
}
static bool read97_stop(struct nf89_queue *q){
 if(q->read97.active&&po96_cancel(q->files->fs,&q->read97)!=PO64_CANCELLED)return false;
 q->read97_id=0;q->read97_seen=0;return true;
}
size_t nf97_reconcile(struct nf89_queue *q){
 if(!qready(q)||q->busy||q->files->busy)return 0;
 size_t n=0;
 if(q->read97_id){
  struct nf89_request *r=read97_request(q);struct nf85_owner o;
  bool live=r&&owner(q->files,r->owner.pid,r->owner.handle,r->owner.root,false,&o)&&same(o,r->owner);
  struct nf85_grant *g=live?find(q->files,o,r->token):0;
  bool valid=g&&g->mount==r->mount&&token_live(q->files,g)&&(g->rights&PO64_READ);
  bool selected=live&&owner(q->files,o.pid,o.handle,o.root,true,&o);
  if(!valid||!selected){
   if(!read97_stop(q))return 0;
   if(r){
    wipe(r->data,sizeof r->data);
    if(!live)wipe(r,sizeof *r);
    else if(!valid){r->bytes=0;r->status=PO64_STALE;r->state=NF89_DONE;}
    /* Selected session may resume the pending request from the start. */
   }
   ++n;
  }
 }
 return n+nf89_reconcile(q);
}
static bool read97_sink(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 struct nf89_queue *q=ctx;struct nf89_request *r=read97_request(q);
 if(!q->busy||!r||at!=q->read97_seen||q->read97_seen>r->bytes||n>r->bytes-q->read97_seen)return false;
 for(size_t i=0;i<n;++i)r->data[q->read97_seen+i]=p[i];
 q->read97_seen+=n;return true;
}
static void read97_complete(struct nf89_queue *q,struct nf89_request *r,enum po64_status status){
 size_t bytes=status==PO64_OK?q->read97_seen:0;
 if(status!=PO64_OK)wipe(r->data,sizeof r->data);
 r->bytes=bytes;r->status=status;r->state=NF89_DONE;
 q->read97_id=0;q->read97_seen=0;
}
bool nf97_pump(struct nf89_queue *q){
 if(!qready(q)||q->busy||q->files->busy)return false;
 (void)nf97_reconcile(q);
 struct nf89_request *r=read97_request(q);
 if(q->read97_id){
  if(!r||!q->read97.active)return false;
  bool done=false;q->busy=true;
  enum po64_status status=po96_step(q->files->fs,&q->read97,read97_sink,q,&done);
  q->busy=false;
  if(status==PO64_OK&&!done)return true;
  if(q->read97.active&&!read97_stop(q))return false;
  read97_complete(q,r,status);
  return true;
 }
 if(q->files->fs->busy)return false;
 for(unsigned i=0;i<NF89_SLOTS;++i){
  struct nf89_request *p=&q->slots[i];struct nf85_owner o;
  if(p->state!=NF89_PENDING||!owner(q->files,p->owner.pid,p->owner.handle,p->owner.root,true,&o)||
    !same(o,p->owner))continue;
  if(!r||p->id<r->id)r=p;
 }
 if(!r)return false;
 if(r->op!=NF89_READ)return nf89_pump(q);
 q->busy=true;
 enum po64_status status=po96_begin(q->files->fs,principal(r->owner.pid),r->token,&q->read97);
 if(status==PO64_OK){
  struct sfs64_map map;
  if(!sfs64_map_decode(q->read97.manifest,q->read97.map_bytes,q->read97.layout.data_sectors,
                      q->read97.layout.data_start,&map))status=PO64_CORRUPT;
  else if(map.file.bytes>r->bytes)status=PO64_FULL;
  else {q->read97_id=r->id;q->read97_seen=0;}
  if(status!=PO64_OK){
   if(po96_cancel(q->files->fs,&q->read97)!=PO64_CANCELLED){q->busy=false;return false;}
  }
 }
 q->busy=false;
 if(status!=PO64_OK)read97_complete(q,r,status);
 return true;
}
static struct nf89_request *write99_request(struct nf89_queue *q){
 for(unsigned i=0;i<NF89_SLOTS;++i)
  if(q->write99_id&&q->slots[i].id==q->write99_id&&q->slots[i].state==NF89_PENDING&&q->slots[i].op==NF89_WRITE)
   return &q->slots[i];
 return 0;
}
static void write99_complete(struct nf89_queue *q,struct nf89_request *r,enum po64_status status){
 if(r){wipe(r->data,sizeof r->data);r->bytes=0;r->status=status;r->state=NF89_DONE;}
 q->write99_id=0;q->write99_staged=false;q->write99_cancel=false;
}
size_t nf99_reconcile(struct nf89_queue *q){
 if(!qready(q)||q->busy||q->files->busy)return 0;
 size_t n=0;
 if(q->write99_id){
  struct nf89_request *r=write99_request(q);struct nf85_owner o;
  bool live=r&&owner(q->files,r->owner.pid,r->owner.handle,r->owner.root,false,&o)&&same(o,r->owner);
  struct nf85_grant *g=live?find(q->files,o,r->token):0;
  bool valid=g&&g->mount==r->mount&&token_live(q->files,g)&&(g->rights&PO64_WRITE);
  bool selected=live&&owner(q->files,o.pid,o.handle,o.root,true,&o);
  if(!valid||!selected||q->write99_cancel){
   q->busy=true;
   enum po64_status rc=po98_cancel(q->files->fs,&q->write99);
   q->busy=false;
   if(q->write99.active)return 0;
   if(rc==PO64_CANCELLED&&!valid)rc=PO64_STALE;
   write99_complete(q,r,rc);
   if(r&&!live)wipe(r,sizeof *r);
   ++n;
  }
 }
 return n+nf97_reconcile(q);
}
static bool write99_source(void *ctx,uint64_t at,uint8_t *p,size_t n){
 struct nf89_queue *q=ctx;struct nf89_request *r=write99_request(q);
 if(!q->busy||!r||at>r->bytes||n>r->bytes-at)return false;
 for(size_t i=0;i<n;++i)p[i]=r->data[(size_t)at+i];
 return true;
}
bool nf99_pump(struct nf89_queue *q){
 if(!qready(q)||q->busy||q->files->busy)return false;
 if(nf99_reconcile(q))return true;
 if(q->read97_id)return nf97_pump(q);
 struct nf89_request *r=write99_request(q);
 if(q->write99_id){
  if(!r||!q->write99.active)return false;
  q->busy=true;
  enum po64_status rc;
  if(q->write99_staged){
   rc=po98_commit(q->files->fs,&q->write99);
  }else{
   bool done=false;
   rc=po98_step(q->files->fs,&q->write99,write99_source,q,&done);
   if(rc==PO64_OK)q->write99_staged=done;
  }
  q->busy=false;
  if(!q->write99.active)write99_complete(q,r,rc);
  return true;
 }
 if(q->files->fs->busy)return false;
 for(unsigned i=0;i<NF89_SLOTS;++i){
  struct nf89_request *p=&q->slots[i];struct nf85_owner o;
  if(p->state!=NF89_PENDING||!owner(q->files,p->owner.pid,p->owner.handle,p->owner.root,true,&o)||
     !same(o,p->owner))continue;
  if(!r||p->id<r->id)r=p;
 }
 if(!r)return false;
 if(r->op==NF89_READ)return nf97_pump(q);
 q->busy=true;
 enum po64_status rc=po98_begin(q->files->fs,principal(r->owner.pid),r->token,r->bytes,&q->write99);
 q->busy=false;
 if(rc==PO64_OK){q->write99_id=r->id;q->write99_staged=false;q->write99_cancel=false;}
 else write99_complete(q,r,rc);
 return true;
}
enum po64_status nf89_cancel(struct nf89_queue *q,uint32_t pid,uint64_t handle,uint64_t root,uint64_t id){
 if(!qready(q)||!id)return PO64_INVALID;
 if(q->busy||q->files->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(q->files,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf89_request *r=qfind(q,o,id);if(!r)return PO64_DENIED;
 if(r->state!=NF89_PENDING)return PO64_BUSY;
 /* A live write cancellation is recorded only. Recovery validation may
  * perform disk I/O and must be deferred to the supervisor. */
 if(q->write99_id==id){q->write99_cancel=true;return PO64_OK;}
 if(q->files->fs->busy&&q->read97_id!=id)return PO64_BUSY;
 if(q->read97_id==id&&!read97_stop(q))return PO64_BUSY;
 wipe(r->data,sizeof r->data);r->bytes=0;r->status=PO64_CANCELLED;r->state=NF89_DONE;
 return PO64_OK;
}
enum po64_status nf89_take(struct nf89_queue *q,uint32_t pid,uint64_t handle,uint64_t root,uint64_t id,
 uint8_t *out,size_t capacity,size_t *bytes,enum po64_status *result){
 if(!qready(q)||!id||!bytes||!result||capacity>NF85_READ_MAX||
    !qspan(q,bytes,sizeof *bytes)||!qspan(q,result,sizeof *result)||!sep(bytes,sizeof *bytes,result,sizeof *result)||
    (capacity&&(!qspan(q,out,capacity)||!sep(out,capacity,bytes,sizeof *bytes)||!sep(out,capacity,result,sizeof *result))))
  return PO64_INVALID;
 if(q->busy||q->files->busy||q->files->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(q->files,pid,handle,root,true,&o))return PO64_DENIED;
 nf89_reconcile(q);
 struct nf89_request *r=qfind(q,o,id);if(!r)return PO64_DENIED;
 if(r->state!=NF89_DONE)return PO64_BUSY;
 if(r->bytes>capacity)return PO64_FULL;
 for(size_t i=0;i<r->bytes;++i)out[i]=r->data[i];
 *bytes=r->bytes;*result=r->status;wipe(r,sizeof *r);return PO64_OK;
}

enum po64_status nf95_wait(struct nf89_queue *q,uint32_t pid,uint64_t handle,uint64_t root,uint64_t id){
 if(!qready(q)||!id)return PO64_INVALID;
 if(q->busy||q->files->busy||q->files->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(q->files,pid,handle,root,false,&o))return PO64_DENIED;
 nf89_reconcile(q);
 struct nf89_request *r=qfind(q,o,id);if(!r)return PO64_DENIED;
 if(!owner(q->files,pid,handle,root,true,&o))return PO64_BUSY;
 return r->state==NF89_DONE?PO64_OK:PO64_BUSY;
}

static bool stream100_ready(const struct nf100_stream *s){return s&&ready(s->files);}
static bool stream100_span(const struct nf100_stream *s,const void *p,size_t n){
 return sep(p,n,s,sizeof *s)&&external_span(s->files,p,n);
}
static bool stream100_owner(struct nf100_stream *s,uint32_t pid,uint64_t handle,uint64_t root,uint64_t id){
 struct nf85_owner o;
 return id&&id==s->id&&owner(s->files,pid,handle,root,true,&o)&&same(o,s->owner);
}
static void stream100_clear(struct nf100_stream *s){
 struct nf85_service *f=s->files;uint64_t serial=s->serial;
 wipe(s,sizeof *s);s->files=f;s->serial=serial;
}
static void stream100_done(struct nf100_stream *s,enum po64_status status){
 wipe(s->data,sizeof s->data);s->buffered=0;s->idle=0;
 s->status=status;s->state=NF100_DONE;s->cancel=false;
}
enum po64_status nf100_start(struct nf100_stream *s,struct nf85_service *f){
 if(!s||s->files||!ready(f)||!external_span(f,s,sizeof *s))return PO64_INVALID;
 wipe(s,sizeof *s);s->files=f;return PO64_OK;
}
enum po64_status nf100_open(struct nf100_stream *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,uint64_t total,uint64_t *id){
 if(!stream100_ready(s)||!token||!stream100_span(s,id,sizeof *id))return PO64_INVALID;
 if(s->busy||s->state!=NF100_IDLE||s->files->busy||s->files->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s->files,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf85_grant *g=find(s->files,o,token);
 if(!g||!(g->rights&PO64_WRITE))return PO64_DENIED;
 if(!token_live(s->files,g))return PO64_STALE;
 if(s->serial==UINT64_MAX)return PO64_FULL;
 s->owner=o;s->id=++s->serial;s->token=token;s->mount=g->mount;s->total=total;
 s->state=NF100_BEGIN;*id=s->id;return PO64_OK;
}
enum po64_status nf100_feed(struct nf100_stream *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t id,uint64_t offset,const uint8_t *data,size_t n){
 if(!stream100_ready(s)||!n||n>NF85_READ_MAX||!stream100_span(s,data,n))return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!stream100_owner(s,pid,handle,root,id))return PO64_DENIED;
 if(s->state!=NF100_NEED||s->cancel)return PO64_BUSY;
 if(offset!=s->offset||offset>=s->total)return PO64_INVALID;
 uint64_t left=s->total-offset;size_t expected=left>NF85_READ_MAX?NF85_READ_MAX:(size_t)left;
 if(n!=expected)return PO64_INVALID;
 for(size_t i=0;i<n;++i)s->data[i]=data[i];
 s->buffered=n;s->idle=0;s->state=NF100_DATA;return PO64_OK;
}
enum po64_status nf100_finish(struct nf100_stream *s,uint32_t pid,uint64_t handle,uint64_t root,uint64_t id){
 if(!stream100_ready(s))return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!stream100_owner(s,pid,handle,root,id))return PO64_DENIED;
 if(s->state!=NF100_STAGED||s->cancel)return PO64_BUSY;
 s->state=NF100_COMMIT;s->idle=0;return PO64_OK;
}
enum po64_status nf100_cancel(struct nf100_stream *s,uint32_t pid,uint64_t handle,uint64_t root,uint64_t id){
 if(!stream100_ready(s))return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!stream100_owner(s,pid,handle,root,id))return PO64_DENIED;
 if(s->state==NF100_DONE)return PO64_BUSY;
 s->cancel=true;return PO64_OK;
}
enum po64_status nf100_result(struct nf100_stream *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t id,enum po64_status *result){
 if(!stream100_ready(s)||!stream100_span(s,result,sizeof *result))return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!stream100_owner(s,pid,handle,root,id))return PO64_DENIED;
 if(s->state!=NF100_DONE)return PO64_BUSY;
 *result=s->status;stream100_clear(s);return PO64_OK;
}
static bool stream100_source(void *ctx,uint64_t at,uint8_t *p,size_t n){
 struct nf100_stream *s=ctx;
 if(!s->busy||s->state!=NF100_DATA||at<s->offset||
    at-s->offset>s->buffered||n>s->buffered-(size_t)(at-s->offset))return false;
 for(size_t i=0;i<n;++i)p[i]=s->data[(size_t)(at-s->offset)+i];
 return true;
}
bool nf100_pump(struct nf100_stream *s){
 if(!stream100_ready(s)||s->busy||s->files->busy||s->state==NF100_IDLE)return false;
 struct nf85_owner o;
 bool live=owner(s->files,s->owner.pid,s->owner.handle,s->owner.root,false,&o)&&same(o,s->owner);
 if(s->state==NF100_DONE){if(!live){stream100_clear(s);return true;}return false;}
 struct nf85_grant *g=live?find(s->files,o,s->token):0;
 bool valid=g&&g->mount==s->mount&&token_live(s->files,g)&&(g->rights&PO64_WRITE);
 bool selected=live&&owner(s->files,o.pid,o.handle,o.root,true,&o);
 bool waiting=s->state==NF100_NEED||s->state==NF100_STAGED;
 if(waiting&&s->idle<NF100_IDLE_LIMIT)++s->idle;
 if(!valid||!selected||s->cancel||(waiting&&s->idle>=NF100_IDLE_LIMIT)){
  enum po64_status rc=PO64_CANCELLED;
  if(s->writer.active){
   s->busy=true;rc=po98_cancel(s->files->fs,&s->writer);s->busy=false;
   if(s->writer.active)return false;
  }
  if(rc==PO64_CANCELLED&&!valid)rc=PO64_STALE;
  stream100_done(s,rc);if(!live)stream100_clear(s);
  return true;
 }
 if(waiting)return false;
 if(s->state==NF100_BEGIN){
  if(s->files->fs->busy)return false;
  s->busy=true;
  enum po64_status rc=po98_begin(s->files->fs,principal(s->owner.pid),s->token,s->total,&s->writer);
  s->busy=false;
  if(rc!=PO64_OK)stream100_done(s,rc);
  else s->state=s->total?NF100_NEED:NF100_STAGED;
  return true;
 }
 if(s->state==NF100_DATA){
  bool done=false;s->busy=true;
  enum po64_status rc=po98_step(s->files->fs,&s->writer,stream100_source,s,&done);
  s->busy=false;
  if(rc!=PO64_OK){if(!s->writer.active)stream100_done(s,rc);return true;}
  if(s->writer.offset!=s->offset+s->buffered||done!=(s->writer.offset==s->total)){
   s->cancel=true;return true;
  }
  s->offset=s->writer.offset;wipe(s->data,sizeof s->data);s->buffered=0;s->idle=0;
  s->state=done?NF100_STAGED:NF100_NEED;return true;
 }
 if(s->state==NF100_COMMIT){
  s->busy=true;enum po64_status rc=po98_commit(s->files->fs,&s->writer);s->busy=false;
  if(!s->writer.active)stream100_done(s,rc);
  return true;
 }
 return false;
}

static bool stream101_ready(const struct nf101_stream *s){return s&&ready(s->files);}
static bool stream101_span(const struct nf101_stream *s,const void *p,size_t n){
 return sep(p,n,s,sizeof *s)&&external_span(s->files,p,n);
}
static bool stream101_owner(struct nf101_stream *s,uint32_t pid,uint64_t handle,uint64_t root,uint64_t id){
 struct nf85_owner o;
 return id&&id==s->id&&owner(s->files,pid,handle,root,true,&o)&&same(o,s->owner);
}
static bool stream101_valid(struct nf101_stream *s){
 struct nf85_grant *g=find(s->files,s->owner,s->token);
 return g&&g->mount==s->mount&&token_live(s->files,g)&&(g->rights&PO64_READ);
}
static void stream101_clear(struct nf101_stream *s){
 struct nf85_service *f=s->files;uint64_t serial=s->serial;
 wipe(s,sizeof *s);s->files=f;s->serial=serial;
}
static void stream101_done(struct nf101_stream *s,enum po64_status status){
 wipe(s->data,sizeof s->data);s->buffered=0;s->idle=0;
 s->state=NF101_DONE;s->status=status;s->cancel=false;
 s->verified=status==PO64_OK;
}
enum po64_status nf101_start(struct nf101_stream *s,struct nf85_service *f){
 if(!s||s->files||!ready(f)||!external_span(f,s,sizeof *s))return PO64_INVALID;
 wipe(s,sizeof *s);s->files=f;return PO64_OK;
}
enum po64_status nf101_open(struct nf101_stream *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,uint64_t *id){
 if(!stream101_ready(s)||!token||!stream101_span(s,id,sizeof *id))return PO64_INVALID;
 if(s->busy||s->state!=NF101_IDLE||s->files->busy||s->files->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s->files,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf85_grant *g=find(s->files,o,token);
 if(!g||!(g->rights&PO64_READ))return PO64_DENIED;
 if(!token_live(s->files,g))return PO64_STALE;
 if(s->serial==UINT64_MAX)return PO64_FULL;
 s->owner=o;s->id=++s->serial;s->token=token;s->mount=g->mount;
 s->state=NF101_BEGIN;*id=s->id;return PO64_OK;
}
enum po64_status nf101_take(struct nf101_stream *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t id,uint64_t offset,uint8_t *out,size_t capacity,size_t *bytes){
 if(!stream101_ready(s)||!capacity||capacity>NF85_READ_MAX||
    !stream101_span(s,out,capacity)||!stream101_span(s,bytes,sizeof *bytes)||
    !sep(out,capacity,bytes,sizeof *bytes))return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!stream101_owner(s,pid,handle,root,id))return PO64_DENIED;
 if(s->state==NF101_DONE)return PO64_END;
 if(s->state!=NF101_READY||s->cancel)return PO64_BUSY;
 if(!stream101_valid(s))return PO64_STALE;
 if(offset!=s->offset)return PO64_INVALID;
 if(capacity<s->buffered)return PO64_FULL;
 for(size_t i=0;i<s->buffered;++i)out[i]=s->data[i];
 *bytes=s->buffered;s->offset+=s->buffered;
 wipe(s->data,sizeof s->data);s->buffered=0;s->idle=0;
 if(s->verified)stream101_done(s,PO64_OK);
 else s->state=NF101_PULL;
 return PO64_OK;
}
enum po64_status nf101_cancel(struct nf101_stream *s,uint32_t pid,uint64_t handle,uint64_t root,uint64_t id){
 if(!stream101_ready(s))return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!stream101_owner(s,pid,handle,root,id))return PO64_DENIED;
 if(s->state==NF101_DONE)return PO64_BUSY;
 s->cancel=true;return PO64_OK;
}
enum po64_status nf101_result(struct nf101_stream *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t id,enum po64_status *result){
 if(!stream101_ready(s)||!stream101_span(s,result,sizeof *result))return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!stream101_owner(s,pid,handle,root,id))return PO64_DENIED;
 if(s->state!=NF101_DONE)return PO64_BUSY;
 *result=s->status==PO64_OK&&!stream101_valid(s)?PO64_STALE:s->status;
 stream101_clear(s);return PO64_OK;
}
static bool stream101_sink(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 struct nf101_stream *s=ctx;
 if(!s->busy||s->state!=NF101_PULL||at<s->offset||at-s->offset!=s->buffered||
    s->buffered>sizeof s->data||n>sizeof s->data-s->buffered)return false;
 for(size_t i=0;i<n;++i)s->data[s->buffered+i]=p[i];
 s->buffered+=n;return true;
}
bool nf101_pump(struct nf101_stream *s){
 if(!stream101_ready(s)||s->busy||s->files->busy||s->state==NF101_IDLE)return false;
 struct nf85_owner o;
 bool live=owner(s->files,s->owner.pid,s->owner.handle,s->owner.root,false,&o)&&same(o,s->owner);
 if(s->state==NF101_DONE){if(!live){stream101_clear(s);return true;}return false;}
 bool valid=live&&stream101_valid(s);
 bool selected=live&&owner(s->files,o.pid,o.handle,o.root,true,&o);
 bool waiting=s->state==NF101_READY;
 if(waiting&&s->idle<NF100_IDLE_LIMIT)++s->idle;
 if(!valid||!selected||s->cancel||(waiting&&s->idle>=NF100_IDLE_LIMIT)){
  if(s->reader.active&&po96_cancel(s->files->fs,&s->reader)!=PO64_CANCELLED)return false;
  stream101_done(s,valid?PO64_CANCELLED:PO64_STALE);
  if(!live)stream101_clear(s);
  return true;
 }
 if(waiting)return false;
 if(s->state==NF101_BEGIN){
  if(s->files->fs->busy)return false;
  s->busy=true;
  enum po64_status rc=po96_begin(s->files->fs,principal(s->owner.pid),s->token,&s->reader);
  s->busy=false;
  if(rc!=PO64_OK){stream101_done(s,rc);return true;}
  struct sfs64_map map;
  if(!sfs64_map_decode(s->reader.manifest,s->reader.map_bytes,s->reader.layout.data_sectors,
       s->reader.layout.data_start,&map)){
   if(po96_cancel(s->files->fs,&s->reader)!=PO64_CANCELLED)return false;
   stream101_done(s,PO64_CORRUPT);return true;
  }
  s->total=map.file.bytes;s->state=NF101_PULL;return true;
 }
 if(s->state==NF101_PULL){
  bool done=false;s->busy=true;
  enum po64_status rc=po96_step(s->files->fs,&s->reader,stream101_sink,s,&done);
  s->busy=false;
  if(rc!=PO64_OK){
   if(s->reader.active&&po96_cancel(s->files->fs,&s->reader)!=PO64_CANCELLED)return false;
   stream101_done(s,rc);return true;
  }
  if(s->offset>s->total||s->buffered>s->total-s->offset||
     done!=(s->buffered==s->total-s->offset)||(!done&&!s->buffered)){
   if(s->reader.active&&po96_cancel(s->files->fs,&s->reader)!=PO64_CANCELLED)return false;
   stream101_done(s,PO64_CORRUPT);return true;
  }
  s->verified=done;s->idle=0;
  if(done&&!s->buffered)stream101_done(s,PO64_OK);
  else s->state=NF101_READY;
  return true;
 }
 return false;
}

static bool stream102_ready(const struct nf102_service *s){
 return s&&qready(s->queue)&&s->read.files==s->queue->files&&s->write.files==s->queue->files;
}
static bool stream102_span(const struct nf102_service *s,const void *p,size_t n){
 return sep(p,n,s,sizeof *s)&&qspan(s->queue,p,n);
}
static bool active102(const struct nf102_service *s){
 return (s->read.state!=NF101_IDLE&&s->read.state!=NF101_DONE)||
        (s->write.state!=NF100_IDLE&&s->write.state!=NF100_DONE);
}
enum po64_status nf102_start(struct nf102_service *s,struct nf89_queue *q){
 if(!s||s->queue||!qready(q)||!qspan(q,s,sizeof *s))return PO64_INVALID;
 wipe(s,sizeof *s);
 if(nf100_start(&s->write,q->files)!=PO64_OK||nf101_start(&s->read,q->files)!=PO64_OK){
  wipe(s,sizeof *s);return PO64_INVALID;
 }
 s->queue=q;return PO64_OK;
}
static bool queued102(const struct nf89_queue *q){
 if(q->read97_id||q->write99_id)return true;
 for(unsigned i=0;i<NF89_SLOTS;++i)if(q->slots[i].state==NF89_PENDING)return true;
 return false;
}
enum po64_status nf102_submit(struct nf102_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,enum nf89_op op,const uint8_t *data,size_t n,uint64_t *request){
 if(!stream102_ready(s)||!stream102_span(s,request,sizeof *request)||
    (data&&n&&!stream102_span(s,data,n)))return PO64_INVALID;
 if(active102(s))return PO64_BUSY;
 return nf89_submit(s->queue,pid,handle,root,token,op,data,n,request);
}
/* Reconciliation only cancels invalid/abandoned ownership, never advances
 * data, ages a waiting client, or commits. Trusted safe-root call only. */
size_t nf102_reconcile(struct nf102_service *s){
 if(!stream102_ready(s)||s->queue->busy||s->read.busy||s->write.busy)return 0;
 size_t n=0;
 struct nf85_service *f=s->queue->files;struct nf85_owner o;
 if(s->read.state!=NF101_IDLE){
  bool live=owner(f,s->read.owner.pid,s->read.owner.handle,s->read.owner.root,false,&o)&&same(o,s->read.owner);
  bool selected=live&&owner(f,o.pid,o.handle,o.root,true,&o);
  if(!live||(s->read.state!=NF101_DONE&&(!selected||!stream101_valid(&s->read)||s->read.cancel)))
   n+=nf101_pump(&s->read);
 }
 if(s->write.state!=NF100_IDLE){
  bool live=owner(f,s->write.owner.pid,s->write.owner.handle,s->write.owner.root,false,&o)&&same(o,s->write.owner);
  struct nf85_grant *g=live?find(f,o,s->write.token):0;
  bool valid=g&&g->mount==s->write.mount&&token_live(f,g)&&(g->rights&PO64_WRITE);
  bool selected=live&&owner(f,o.pid,o.handle,o.root,true,&o);
  if(!live||(s->write.state!=NF100_DONE&&(!selected||!valid||s->write.cancel)))
   n+=nf100_pump(&s->write);
 }
 return n+nf99_reconcile(s->queue);
}
bool nf102_pump(struct nf102_service *s){
 if(!stream102_ready(s))return false;
 if(nf102_reconcile(s))return true;
 if(s->read.state!=NF101_IDLE&&s->read.state!=NF101_DONE)return nf101_pump(&s->read);
 if(s->write.state!=NF100_IDLE&&s->write.state!=NF100_DONE)return nf100_pump(&s->write);
 return nf99_pump(s->queue);
}
enum po64_status nf102_call(struct nf102_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t op,uint64_t token,uint8_t *packet,uint64_t size,uint64_t *value){
 if(!stream102_ready(s)||!token||!stream102_span(s,value,sizeof *value))return PO64_INVALID;
 if(packet&&size<=NF102_PACKET_MAX&&!sep(packet,(size_t)size,value,sizeof *value))return PO64_INVALID;
 *value=0;
 if(op==NF102_ROPEN||op==NF102_WOPEN){
  if(packet||(op==NF102_ROPEN&&size))return PO64_INVALID;
  if(active102(s)||queued102(s->queue))return PO64_BUSY;
  if(op==NF102_ROPEN)return nf101_open(&s->read,pid,handle,root,token,value);
  return nf100_open(&s->write,pid,handle,root,token,size,value);
 }
 if(op==NF102_RTAKE||op==NF102_WFEED){
  if(size<=NF102_HEADER||size>NF102_PACKET_MAX||!stream102_span(s,packet,(size_t)size)||
     !sep(packet,(size_t)size,value,sizeof *value))return PO64_INVALID;
  uint64_t offset=0;
  for(unsigned i=0;i<NF102_HEADER;++i)offset|=(uint64_t)packet[i]<<(8*i);
  size_t bytes=(size_t)size-NF102_HEADER;
  if(op==NF102_WFEED)return nf100_feed(&s->write,pid,handle,root,token,offset,packet+NF102_HEADER,bytes);
  size_t taken=0;
  enum po64_status rc=nf101_take(&s->read,pid,handle,root,token,offset,packet+NF102_HEADER,bytes,&taken);
  if(rc==PO64_OK)*value=taken;
  return rc;
 }
 if(packet||size)return PO64_INVALID;
 switch(op){
 case NF102_WFINISH:return nf100_finish(&s->write,pid,handle,root,token);
 case NF102_RCANCEL:return nf101_cancel(&s->read,pid,handle,root,token);
 case NF102_WCANCEL:return nf100_cancel(&s->write,pid,handle,root,token);
 case NF102_RRESULT:case NF102_WRESULT:{
  enum po64_status result=PO64_INVALID;
  enum po64_status rc=op==NF102_RRESULT?nf101_result(&s->read,pid,handle,root,token,&result):
      nf100_result(&s->write,pid,handle,root,token,&result);
  if(rc==PO64_OK)*value=result;
  return rc;
 }
 default:return PO64_INVALID;
 }
}
