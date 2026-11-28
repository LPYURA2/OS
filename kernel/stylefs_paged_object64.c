#include <styleos/stylefs_paged_object64.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static enum po64_status status(enum pp64_status rc){
 switch(rc){
 case PP64_OK:return PO64_OK;
 case PP64_FULL:return PO64_FULL;
 case PP64_IO:return PO64_IO;
 case PP64_UNCERTAIN:return PO64_UNCERTAIN;
 case PP64_CORRUPT:return PO64_CORRUPT;
 case PP64_CANCELLED:return PO64_CANCELLED;
 default:return PO64_STALE;
 }
}
static void forget(struct po64_service *s){
 for(unsigned i=0;i<PO64_HANDLES;i++)s->handles[i]=(struct po64_handle){0};
}
static void clear(struct po64_service *s){forget(s);s->reader=0;s->active=false;}
static bool mode(struct bs_service *b,const struct pp64_layout *l,bool write){
 const uint64_t tokens[]={l->receipt,l->dir,l->inode,l->data};
 for(unsigned i=0;i<4;i++){
  bool found=false;
  for(unsigned j=0;j<BS_GRANTS;j++){
   const struct bs_grant *g=&b->grants[j];
   if(g->token==tokens[i]&&g->owner==l->owner&&g->rights==(write?(BS_READ|BS_WRITE):BS_READ)&&g->token){
    found=true;break;
   }
  }
  if(!found)return false;
 }
 return true;
}
static bool span(struct po64_service *s,const void *p,size_t n){
 return sep(p,n,s,sizeof *s)&&sep(p,n,s->block,sizeof *s->block)&&
 sep(p,n,s->work,sizeof *s->work)&&sep(p,n,s->nodes,s->node_cap*sizeof *s->nodes)&&
 sep(p,n,s->runs,s->run_cap*sizeof *s->runs)&&
 (!s->writable||sep(p,n,s->previous,s->run_cap*sizeof *s->previous));
}
static enum po64_status mount(struct po64_service *s,struct bs_service *b,const struct pp64_layout *l,
 struct pp64_stream_workspace *w,struct dc64_node *a,size_t cap,struct sfs64_run *runs,struct sfs64_run *previous,size_t rcap,bool write){
 if(!s)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(s->active||!cap||!rcap||cap>SIZE_MAX/sizeof *a||rcap>SIZE_MAX/sizeof *runs)return PO64_INVALID;
 const void *p[]={s,b,l,w,a,runs,previous};size_t n[]={sizeof *s,sizeof *b,sizeof *l,sizeof *w,cap*sizeof *a,rcap*sizeof *runs,rcap*sizeof *previous};
 for(unsigned i=0;i<(write?7u:6u);i++)for(unsigned j=0;j<i;j++)if(!sep(p[i],n[i],p[j],n[j]))return PO64_INVALID;
 if(!mode(b,l,write))return PO64_DENIED;
 if(s->serial==UINT64_MAX)return PO64_FULL;
 struct pp64_pair pair;
 enum pp64_status rc=pp64_mount(b,l,&w->pair,a,cap,runs,rcap,&pair);
 if(rc!=PP64_OK)return status(rc);
 clear(s);s->block=b;s->layout=*l;s->pair=pair;s->work=w;s->nodes=a;s->node_cap=cap;s->runs=runs;s->run_cap=rcap;
 s->mount_id=++s->serial;s->writable=write;s->previous=previous;s->active=true;return PO64_OK;
}
enum po64_status po64_mount(struct po64_service *s,struct bs_service *b,const struct pp64_layout *l,
 struct pp64_stream_workspace *w,struct dc64_node *a,size_t cap,struct sfs64_run *runs,size_t rcap){
 return mount(s,b,l,w,a,cap,runs,0,rcap,false);
}
enum po64_status po64_mount_rw(struct po64_service *s,struct bs_service *b,const struct pp64_layout *l,
 struct pp64_stream_workspace *w,struct dc64_node *a,size_t cap,struct sfs64_run *runs,struct sfs64_run *previous,size_t rcap){
 return mount(s,b,l,w,a,cap,runs,previous,rcap,true);
}
enum po64_status po64_unmount(struct po64_service *s){
 if(!s)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 clear(s);return PO64_OK;
}
static enum po64_status live(struct po64_service *s){
 if(!s->active)return PO64_STALE;
 if(!mode(s->block,&s->layout,s->writable)){clear(s);return PO64_STALE;}
 struct pp64_pair p;
 enum pp64_status rc=pp64_mount(s->block,&s->layout,&s->work->pair,s->nodes,s->node_cap,s->runs,s->run_cap,&p);
 if(rc!=PP64_OK){clear(s);return status(rc);}
 if(p.sequence!=s->pair.sequence||p.dir_epoch!=s->pair.dir_epoch||p.inode_epoch!=s->pair.inode_epoch||p.slot!=s->pair.slot||p.id_floor!=s->pair.id_floor){
  clear(s);return PO64_STALE;
 }
 return PO64_OK;
}
enum po64_status po64_grant_access(struct po64_service *s,uint64_t principal,const struct sd64_key *key,uint64_t id,uint64_t rev,unsigned rights,uint64_t *token){
 if(!s||!principal||id<=1||!rev||!rights||(rights&~(PO64_READ|PO64_WRITE)))return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 if((rights&PO64_WRITE)&&!s->writable)return PO64_DENIED;
 if(!span(s,key,sizeof *key)||!span(s,token,sizeof *token)||!sep(key,sizeof *key,token,sizeof *token)||!sd64_key_valid(key))return PO64_INVALID;
 enum po64_status rc=live(s);if(rc!=PO64_OK)return rc;
 struct dc64_state *d=&s->work->pair.dir;uint64_t found=0;uint8_t kind=0;
 enum sd64_status dr=d->count?sd64_lookup(s->block,s->layout.owner,s->layout.dir,&d->root,key,s->work->pair.dw.page,&found,&kind):SD64_MISSING;
 if(dr==SD64_IO){clear(s);return PO64_IO;}
 if(dr==SD64_CORRUPT){clear(s);return PO64_CORRUPT;}
 if(dr!=SD64_OK||found!=id||kind!=SD64_FILE)return PO64_DENIED;
 struct sfs64_file f;
 enum ip64_status ir=ip64_stat(s->block,s->layout.owner,s->layout.inode,&s->work->pair.inode.root,id,&s->work->pair.iw,&f);
 if(ir==IP64_IO){clear(s);return PO64_IO;}
 if(ir!=IP64_OK){clear(s);return PO64_CORRUPT;}
 if(f.revision!=rev)return PO64_STALE;
 for(unsigned i=0;i<PO64_HANDLES;i++)if(!s->handles[i].token){
  if(s->serial==UINT64_MAX)return PO64_FULL;
  uint64_t t=++s->serial;
  s->handles[i]=(struct po64_handle){t,principal,id,rev,*key,rights};
  *token=t;return PO64_OK;
 }
 return PO64_FULL;
}
enum po64_status po64_grant(struct po64_service *s,uint64_t principal,const struct sd64_key *key,uint64_t id,uint64_t rev,uint64_t *token){
 return po64_grant_access(s,principal,key,id,rev,PO64_READ,token);
}
static struct po64_handle *find(struct po64_service *s,uint64_t principal,uint64_t token){
 for(unsigned i=0;i<PO64_HANDLES;i++)if(s->handles[i].token==token&&s->handles[i].principal==principal)return &s->handles[i];
 return 0;
}
enum po64_status po64_revoke(struct po64_service *s,uint64_t principal,uint64_t token){
 if(!s||!principal||!token)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 struct po64_handle *h=find(s,principal,token);if(!h)return PO64_DENIED;
 *h=(struct po64_handle){0};return PO64_OK;
}
enum po64_status po64_revoke_owner(struct po64_service *s,uint64_t principal){
 if(!s||!principal)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 for(unsigned i=0;i<PO64_HANDLES;i++)if(s->handles[i].principal==principal)s->handles[i]=(struct po64_handle){0};
 return PO64_OK;
}
enum po64_status po64_read(struct po64_service *s,uint64_t principal,uint64_t token,pp64_sink sink,void *ctx){
 if(!s||!principal||!token||!sink)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 struct po64_handle *h=find(s,principal,token);if(!h||!(h->rights&PO64_READ))return PO64_DENIED;
 enum po64_status rc=live(s);if(rc!=PO64_OK)return rc;
 /* Key copied to local trusted span; callback reentry cannot revoke or
  * change token. The raw service is never exposed to applications. */
 struct po64_handle copy=*h;
 s->busy=true;
 enum pp64_status pr=pp64_get(s->block,&s->layout,s->work,s->nodes,s->node_cap,s->runs,s->run_cap,
  s->pair.sequence,&copy.key,copy.id,copy.revision,sink,ctx);
 s->busy=false;
 rc=status(pr);
 if(rc!=PO64_OK&&rc!=PO64_CANCELLED)clear(s);
 return rc;
}

enum po64_status po96_begin(struct po64_service *s,uint64_t principal,uint64_t token,struct pp96_reader *r){
 if(!s||!principal||!token||!r)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 if(!span(s,r,sizeof *r)||r->active||r->busy)return PO64_INVALID;
 struct po64_handle *h=find(s,principal,token);
 if(!h||!(h->rights&PO64_READ))return PO64_DENIED;
 enum po64_status rc=live(s);if(rc!=PO64_OK)return rc;
 struct po64_handle copy=*h;
 enum pp64_status pr=pp96_begin(r,s->block,&s->layout,s->work,s->nodes,s->node_cap,
    s->runs,s->run_cap,s->pair.sequence,&copy.key,copy.id,copy.revision);
 rc=status(pr);
 if(rc!=PO64_OK){clear(s);return rc;}
 s->reader=r;s->busy=true;return PO64_OK;
}
enum po64_status po96_step(struct po64_service *s,struct pp96_reader *r,pp64_sink sink,void *ctx,bool *done){
 if(!s||!r||!sink||!done)return PO64_INVALID;
 if(!s->active||!s->busy||s->reader!=r)return PO64_DENIED;
 if(r->busy)return PO64_BUSY;
 if(!span(s,done,sizeof *done)||!sep(done,sizeof *done,r,sizeof *r))return PO64_INVALID;
 enum po64_status rc=status(pp96_step(r,sink,ctx,done));
 if(!r->active){
  s->reader=0;s->busy=false;
  if(rc!=PO64_OK&&rc!=PO64_CANCELLED)clear(s);
 }
 return rc;
}
enum po64_status po96_cancel(struct po64_service *s,struct pp96_reader *r){
 if(!s||!r)return PO64_INVALID;
 if(!s->active||!s->busy||s->reader!=r)return PO64_DENIED;
 if(r->busy)return PO64_BUSY;
 enum po64_status rc=status(pp96_cancel(r));
 if(rc==PO64_CANCELLED){s->reader=0;s->busy=false;}
 return rc;
}

enum po64_status po64_write(struct po64_service *s,uint64_t principal,uint64_t token,uint64_t bytes,pp64_source source,void *ctx){
 if(!s||!principal||!token||(bytes&&!source))return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 struct po64_handle *h=find(s,principal,token);
 if(!s->writable||!h||!(h->rights&PO64_WRITE))return PO64_DENIED;
 enum po64_status rc=live(s);if(rc!=PO64_OK)return rc;
 struct po64_handle copy=*h;struct pp64_pair next;
 s->busy=true;
 enum pp64_status pr=pp64_put(s->block,&s->layout,s->work,s->nodes,s->node_cap,s->runs,s->previous,s->run_cap,
  s->pair.sequence,&copy.key,copy.id,copy.revision,bytes,source,ctx,&next);
 s->busy=false;
 if(pr==PP64_OK){
  s->pair=next;forget(s);return PO64_OK;
 }
 if(pr==PP64_CANCELLED){
  rc=live(s);return rc==PO64_OK?PO64_CANCELLED:rc;
 }
 /* Any other failure after staging began must not expose stale handles.
  * In particular an uncertain publication is never silently retried. */
 clear(s);return status(pr);
}

enum po64_status po64_snapshot(struct po64_service *s,struct po64_snapshot *out){
 if(!s)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 if(!span(s,out,sizeof *out))return PO64_INVALID;
 enum po64_status rc=live(s);if(rc!=PO64_OK)return rc;
 *out=(struct po64_snapshot){s->mount_id,s->pair.sequence};
 return PO64_OK;
}
static enum po64_status row_read(struct po64_service *s,uint64_t index,struct sd64_record *out){
 enum dc64_status rc=dc64_entry_at(&s->work->pair.dir,index,&s->work->pair.dw,out);
 if(rc==DC64_OK)return PO64_OK;
 clear(s);return rc==DC64_IO?PO64_IO:PO64_CORRUPT;
}
enum po64_status po64_child_at(struct po64_service *s,struct po64_snapshot snap,uint64_t parent,uint64_t ordinal,struct po64_row *out){
 if(!s||!parent||!snap.mount||!snap.sequence)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 if(!span(s,out,sizeof *out))return PO64_INVALID;
 if(snap.mount!=s->mount_id||snap.sequence!=s->pair.sequence)return PO64_STALE;
 enum po64_status rc=live(s);if(rc!=PO64_OK)return rc;
 uint64_t count=s->work->pair.dir.count;
 /* Mount verified sorted unique object inventory and every parent edge. */
 if(parent!=1){
  size_t lo=0,hi=(size_t)count;
  while(lo<hi){size_t m=lo+(hi-lo)/2;if(s->nodes[m].id<parent)lo=m+1;else hi=m;}
  if(lo==(size_t)count||s->nodes[lo].id!=parent||s->nodes[lo].kind!=SD64_DIRECTORY)return PO64_DENIED;
 }
 /* Directory keys sorted by parent then name: find the first child,
  * without scanning all earlier directories or narrowing 64-bit IDs. */
 uint64_t lo=0,hi=count;struct sd64_record e;
 while(lo<hi){
  uint64_t m=lo+(hi-lo)/2;
  rc=row_read(s,m,&e);if(rc!=PO64_OK)return rc;
  if(e.key.parent<parent)lo=m+1;else hi=m;
 }
 if(ordinal>=count-lo)return PO64_END;
 rc=row_read(s,lo+ordinal,&e);if(rc!=PO64_OK)return rc;
 if(e.key.parent!=parent)return PO64_END;
 struct po64_row row={.entry=e};
 if(e.kind==SD64_FILE){
  struct sfs64_file f;
  enum ip64_status ir=ip64_stat(s->block,s->layout.owner,s->layout.inode,&s->work->pair.inode.root,e.target,&s->work->pair.iw,&f);
  if(ir!=IP64_OK){clear(s);return ir==IP64_IO?PO64_IO:PO64_CORRUPT;}
  row.bytes=f.bytes;row.revision=f.revision;
 }
 *out=row;return PO64_OK;
}

/* Namespace calls are trusted supervisor requests, never token operations. */
static enum po64_status edit_ready(struct po64_service *s,struct po64_snapshot snap){
 if(!s||!snap.mount||!snap.sequence)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 if(!s->writable)return PO64_DENIED;
 if(snap.mount!=s->mount_id||snap.sequence!=s->pair.sequence)return PO64_STALE;
 return live(s);
}
/* ID zero requests automatic allocation only on explicitly formatted
 * ID-managed volumes. Legacy callers retain the old explicit contract. */
static enum po64_status edit_id(struct po64_service *s,uint64_t *id){
 if(s->pair.id_floor){
  if(s->pair.id_floor==UINT64_MAX)return PO64_FULL;
  uint64_t next=s->pair.id_floor+1;
  if(*id&&*id!=next)return PO64_DENIED;
  *id=next;
 }
 return *id>1?PO64_OK:PO64_INVALID;
}
static bool edit_parent(struct po64_service *s,uint64_t parent){
 if(parent==1)return true;
 for(size_t i=0;i<(size_t)s->work->pair.dir.count;i++)
  if(s->nodes[i].id==parent)return s->nodes[i].kind==SD64_DIRECTORY;
 return false;
}
static enum po64_status edit_lookup(struct po64_service *s,const struct sd64_key *key,
 uint64_t *id,uint8_t *kind){
 struct dc64_state *d=&s->work->pair.dir;
 if(!d->count)return PO64_END;
 enum sd64_status r=sd64_lookup(s->block,s->layout.owner,s->layout.dir,&d->root,key,
  s->work->pair.dw.page,id,kind);
 if(r==SD64_OK)return PO64_OK;
 if(r==SD64_MISSING)return PO64_END;
 clear(s);return r==SD64_IO?PO64_IO:PO64_CORRUPT;
}
static enum po64_status edit_finish(struct po64_service *s,enum pp64_status pr,
 const struct pp64_pair *next){
 s->busy=false;
 if(pr==PP64_OK){s->pair=*next;forget(s);return PO64_OK;}
 /* Staging may have changed an unpublished root. Fail closed, requiring
  * supervisor recovery rather than retaining tokens or silently retrying. */
 clear(s);return status(pr);
}
enum po64_status po64_create_empty(struct po64_service *s,struct po64_snapshot snap,
 const struct sd64_key *key,uint64_t id){
 enum po64_status rc=edit_ready(s,snap);
 if(rc!=PO64_OK)return rc;
 if(!span(s,key,sizeof *key)||!sd64_key_valid(key))return PO64_INVALID;
 rc=edit_id(s,&id);if(rc!=PO64_OK)return rc;
 if(!edit_parent(s,key->parent))return PO64_DENIED;
 for(size_t i=0;i<(size_t)s->work->pair.dir.count;i++)
  if(s->nodes[i].id==id)return PO64_DENIED;
 uint64_t found=0;uint8_t kind=0;
 rc=edit_lookup(s,key,&found,&kind);
 if(rc!=PO64_END)return rc==PO64_OK?PO64_DENIED:rc;
 struct sd64_key copy=*key;struct pp64_pair next;
 s->busy=true;
 enum pp64_status pr=pp64_put(s->block,&s->layout,s->work,s->nodes,s->node_cap,
  s->runs,s->previous,s->run_cap,snap.sequence,&copy,id,0,0,0,0,&next);
 return edit_finish(s,pr,&next);
}
enum po64_status po64_change_file(struct po64_service *s,struct po64_snapshot snap,
 const struct po64_row *old,const struct sd64_key *dest){
 enum po64_status rc=edit_ready(s,snap);
 if(rc!=PO64_OK)return rc;
 if(!span(s,old,sizeof *old)||old->entry.kind!=SD64_FILE||old->entry.target<=1||
    !old->revision||!sd64_key_valid(&old->entry.key)||
    (dest&&(!span(s,dest,sizeof *dest)||!sd64_key_valid(dest))))return PO64_INVALID;
 uint64_t id=0;uint8_t kind=0;
 rc=edit_lookup(s,&old->entry.key,&id,&kind);
 if(rc!=PO64_OK)return rc==PO64_END?PO64_STALE:rc;
 if(id!=old->entry.target||kind!=SD64_FILE)return PO64_STALE;
 struct sfs64_file file;
 enum ip64_status ir=ip64_stat(s->block,s->layout.owner,s->layout.inode,
  &s->work->pair.inode.root,id,&s->work->pair.iw,&file);
 if(ir!=IP64_OK){clear(s);return ir==IP64_IO?PO64_IO:PO64_CORRUPT;}
 if(file.revision!=old->revision||file.bytes!=old->bytes)return PO64_STALE;
 struct dc64_edit e={.op=dest?DC64_RENAME:DC64_REMOVE,.entry=old->entry};
 if(dest){
  if(!edit_parent(s,dest->parent))return PO64_DENIED;
  rc=edit_lookup(s,dest,&id,&kind);
  if(rc!=PO64_END)return rc==PO64_OK?PO64_DENIED:rc;
  e.destination=*dest;
 }
 struct pp64_pair next;
 s->busy=true;
 enum pp64_status pr=pp64_edit(s->block,&s->layout,s->work,s->nodes,s->node_cap,
  s->runs,s->run_cap,snap.sequence,&e,old->revision,&next);
 return edit_finish(s,pr,&next);
}

enum po64_status po64_mkdir(struct po64_service *s,struct po64_snapshot snap,
 const struct sd64_key *key,uint64_t id){
 enum po64_status rc=edit_ready(s,snap);
 if(rc!=PO64_OK)return rc;
 if(!span(s,key,sizeof *key)||!sd64_key_valid(key))return PO64_INVALID;
 rc=edit_id(s,&id);if(rc!=PO64_OK)return rc;
 if(!edit_parent(s,key->parent))return PO64_DENIED;
 for(size_t i=0;i<(size_t)s->work->pair.dir.count;i++)
  if(s->nodes[i].id==id)return PO64_DENIED;
 if(s->work->pair.dir.count>=s->node_cap)return PO64_FULL;
 uint64_t found=0;uint8_t kind=0;
 rc=edit_lookup(s,key,&found,&kind);
 if(rc!=PO64_END)return rc==PO64_OK?PO64_DENIED:rc;
 struct dc64_edit e={.op=DC64_CREATE,.entry={*key,id,SD64_DIRECTORY}};
 struct pp64_pair next;
 s->busy=true;
 enum pp64_status pr=pp64_edit(s->block,&s->layout,s->work,s->nodes,s->node_cap,
  s->runs,s->run_cap,snap.sequence,&e,0,&next);
 return edit_finish(s,pr,&next);
}
/* The selected graph is already validated and sorted. Check ancestry
 * before staging, including empty-directory removal. Lower layers repeat
 * full graph validation including the depth of all moved descendants. */
static bool directory_cycle(struct po64_service *s,uint64_t parent,uint64_t id){
 for(unsigned depth=0;depth<=DC64_PATH_MAX;depth++){
  if(parent==id)return true;
  if(parent==1)return false;
  bool found=false;
  for(size_t i=0;i<(size_t)s->work->pair.dir.count;i++)
   if(s->nodes[i].id==parent&&s->nodes[i].kind==SD64_DIRECTORY){
    parent=s->nodes[i].parent;found=true;break;
   }
  if(!found)return true;
 }
 return true;
}
enum po64_status po64_change_directory(struct po64_service *s,struct po64_snapshot snap,
 const struct po64_row *old,const struct sd64_key *dest){
 enum po64_status rc=edit_ready(s,snap);
 if(rc!=PO64_OK)return rc;
 if(!span(s,old,sizeof *old)||old->entry.kind!=SD64_DIRECTORY||
    old->entry.target<=1||old->revision||old->bytes||!sd64_key_valid(&old->entry.key)||
    (dest&&(!span(s,dest,sizeof *dest)||!sd64_key_valid(dest))))return PO64_INVALID;
 uint64_t id=0;uint8_t kind=0;
 rc=edit_lookup(s,&old->entry.key,&id,&kind);
 if(rc!=PO64_OK)return rc==PO64_END?PO64_STALE:rc;
 if(id!=old->entry.target||kind!=SD64_DIRECTORY)return PO64_STALE;
 struct dc64_edit e={.op=dest?DC64_RENAME:DC64_REMOVE,.entry=old->entry};
 if(dest){
  if(!edit_parent(s,dest->parent)||directory_cycle(s,dest->parent,id))return PO64_DENIED;
  rc=edit_lookup(s,dest,&id,&kind);
  if(rc!=PO64_END)return rc==PO64_OK?PO64_DENIED:rc;
  e.destination=*dest;
 }else{
  for(size_t i=0;i<(size_t)s->work->pair.dir.count;i++)
   if(s->nodes[i].parent==id)return PO64_DENIED;
 }
 struct pp64_pair next;
 s->busy=true;
 enum pp64_status pr=pp64_edit(s->block,&s->layout,s->work,s->nodes,s->node_cap,
  s->runs,s->run_cap,snap.sequence,&e,0,&next);
 return edit_finish(s,pr,&next);
}

enum po64_status po64_copy_file(struct po64_service *s,struct po64_snapshot snap,
 const struct po64_row *old,const struct sd64_key *dest,uint64_t new_id){
 enum po64_status rc=edit_ready(s,snap);
 if(rc!=PO64_OK)return rc;
 if(!span(s,old,sizeof *old)||!span(s,dest,sizeof *dest)||
    old->entry.kind!=SD64_FILE||old->entry.target<=1||!old->revision||
    !sd64_key_valid(&old->entry.key)||!sd64_key_valid(dest))return PO64_INVALID;
 rc=edit_id(s,&new_id);if(rc!=PO64_OK)return rc;
 if(!edit_parent(s,dest->parent))return PO64_DENIED;
 if(s->work->pair.dir.count>=s->node_cap)return PO64_FULL;
 for(size_t i=0;i<(size_t)s->work->pair.dir.count;i++)
  if(s->nodes[i].id==new_id)return PO64_DENIED;
 uint64_t id=0;uint8_t kind=0;
 rc=edit_lookup(s,&old->entry.key,&id,&kind);
 if(rc!=PO64_OK)return rc==PO64_END?PO64_STALE:rc;
 if(id!=old->entry.target||kind!=SD64_FILE)return PO64_STALE;
 struct sfs64_file file;
 enum ip64_status ir=ip64_stat(s->block,s->layout.owner,s->layout.inode,
  &s->work->pair.inode.root,id,&s->work->pair.iw,&file);
 if(ir!=IP64_OK){clear(s);return ir==IP64_IO?PO64_IO:PO64_CORRUPT;}
 if(file.revision!=old->revision||file.bytes!=old->bytes)return PO64_STALE;
 rc=edit_lookup(s,dest,&id,&kind);
 if(rc!=PO64_END)return rc==PO64_OK?PO64_DENIED:rc;
 struct sd64_key src=old->entry.key,dst=*dest;struct pp64_pair next;
 s->busy=true;
 enum pp64_status pr=pp64_copy(s->block,&s->layout,s->work,s->nodes,s->node_cap,
  s->runs,s->previous,s->run_cap,snap.sequence,&src,old->entry.target,old->revision,old->bytes,&dst,new_id,&next);
 return edit_finish(s,pr,&next);
}
