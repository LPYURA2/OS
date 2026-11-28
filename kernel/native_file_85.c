#include <styleos/native_file_85.h>
static uint64_t principal(uint32_t pid){return UINT64_C(0x8000000000000000)|pid;}
static void wipe(void *p,size_t n){volatile uint8_t *b=p;for(size_t i=0;i<n;++i)b[i]=0;}
static bool sep(const void *a,size_t n,const void *b,size_t m){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&n<=UINTPTR_MAX-x&&m<=UINTPTR_MAX-y&&(x+n<=y||y+m<=x);
}
static bool ready(const struct nf85_service *s){
 return s&&s->ready&&s->fs&&s->sessions&&s->sessions->ready&&s->sessions->processes;
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
  if(h->token==g->token&&h->principal==principal(g->owner.pid)&&h->rights==PO64_READ)return true;
 }
 return false;
}
static struct nf85_grant *find(struct nf85_service *s,struct nf85_owner o,uint64_t token){
 for(unsigned i=0;i<NF85_HANDLES;++i)
  if(s->grants[i].token==token&&token&&same(s->grants[i].owner,o))return &s->grants[i];
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
enum po64_status nf85_grant_read(struct nf85_service *s,uint32_t pid,uint64_t handle,uint64_t root,
 const struct sd64_key *key,uint64_t id,uint64_t revision,uint64_t *token){
 if(!ready(s)||!key||!token||!sep(token,sizeof *token,s,sizeof *s)||
    !sep(key,sizeof *key,s,sizeof *s)||!sep(key,sizeof *key,token,sizeof *token))return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 nf85_reconcile(s);
 unsigned slot=NF85_HANDLES;
 for(unsigned i=0;i<NF85_HANDLES;++i)if(!s->grants[i].token){slot=i;break;}
 if(slot==NF85_HANDLES)return PO64_FULL;
 uint64_t value=0;
 enum po64_status rc=po64_grant(s->fs,principal(pid),key,id,revision,&value);
 if(rc!=PO64_OK)return rc;
 s->grants[slot]=(struct nf85_grant){o,value,s->fs->mount_id};*token=value;return PO64_OK;
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
    !sep(out,capacity,s,sizeof *s)||!sep(bytes,sizeof *bytes,s,sizeof *s)||
    !sep(out,capacity,bytes,sizeof *bytes))return PO64_INVALID;
 if(s->busy||s->fs->busy)return PO64_BUSY;
 struct nf85_owner o;if(!owner(s,pid,handle,root,true,&o))return PO64_DENIED;
 struct nf85_grant *g=find(s,o,token);
 if(!g)return PO64_DENIED;
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
