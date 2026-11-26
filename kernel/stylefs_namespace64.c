#include <styleos/stylefs_namespace64.h>
static bool separate(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static enum ns64_status convert(enum dc64_status r){
 switch(r){
 case DC64_OK:return NS64_OK;
 case DC64_MISSING:return NS64_MISSING;
 case DC64_STALE:return NS64_STALE;
 case DC64_FULL:return NS64_FULL;
 case DC64_CORRUPT:return NS64_CORRUPT;
 case DC64_IO:return NS64_IO;
 case DC64_UNCERTAIN:return NS64_UNCERTAIN;
 case DC64_CONFLICT:return NS64_CONFLICT;
 default:return NS64_INVALID;
 }
}
static struct ns64_node *find(struct ns64_node *p,size_t n,uint64_t id){
 size_t lo=0,hi=n;
 while(lo<hi){size_t m=lo+(hi-lo)/2;if(p[m].id<id)lo=m+1;else hi=m;}
 return lo<n&&p[lo].id==id?p+lo:0;
}
static void swap(struct ns64_node *a,struct ns64_node *b){struct ns64_node t=*a;*a=*b;*b=t;}
static void sift(struct ns64_node *a,size_t n,size_t i){
 while(i<n/2){
  size_t j=i*2+1;if(j+1<n&&a[j+1].id>a[j].id)++j;
  if(a[i].id>=a[j].id)return;
  swap(a+i,a+j);i=j;
 }
}
static void sort(struct ns64_node *a,size_t n){
 for(size_t i=n/2;i;i--)sift(a,n,i-1);
 for(size_t i=n;i>1;i--){swap(a,a+i-1);sift(a,i-1,0);}
}
static bool live(const struct dc64_state *s,const struct ns64_proof *p){
 return s&&p&&p->valid&&s->valid&&p->source==s&&p->epoch==s->epoch&&
 p->token==s->token&&p->count==s->count&&p->count<=SIZE_MAX/sizeof(struct ns64_node);
}
enum ns64_status ns64_validate(struct dc64_state *s,struct dc64_workspace *w,
 struct ns64_node *nodes,size_t slots,struct ns64_proof *p){
 if(!s||!s->valid||!s->block||slots>SIZE_MAX/sizeof *nodes||
    !separate(p,sizeof *p,s,sizeof *s)||!separate(p,sizeof *p,w,sizeof *w)||
    !separate(w,sizeof *w,s,sizeof *s)||!separate(w,sizeof *w,s->block,sizeof *s->block)||
    !separate(p,sizeof *p,s->block,sizeof *s->block)||
    !separate(nodes,slots*sizeof *nodes,p,sizeof *p)||
    !separate(nodes,slots*sizeof *nodes,s,sizeof *s)||
    !separate(nodes,slots*sizeof *nodes,w,sizeof *w)||
    !separate(nodes,slots*sizeof *nodes,s->block,sizeof *s->block))return NS64_INVALID;
 *p=(struct ns64_proof){0};
 if(s->count>slots)return NS64_FULL;
 size_t n=(size_t)s->count;
 for(size_t i=0;i<n;){
  struct sd64_record rec;
  enum dc64_status rc=dc64_entry_at(s,w,s->epoch,i,&rec);
  if(rc!=DC64_OK)return convert(rc);
  unsigned count=n-i<SD64_FANOUT?(unsigned)(n-i):SD64_FANOUT;
  for(unsigned j=0;j<count;j++){
   const struct sd64_record *e=w->records+j;
   if(e->target<=NS64_ROOT||!sd64_key_valid(&e->key)||
      (e->kind!=SD64_FILE&&e->kind!=SD64_DIRECTORY))return NS64_CORRUPT;
   nodes[i+j]=(struct ns64_node){e->target,e->key.parent,e->kind,0};
  }i+=count;
 }
 sort(nodes,n);
 for(size_t i=0;i<n;i++){
  if(i&&nodes[i-1].id==nodes[i].id)return NS64_CORRUPT;
  if(nodes[i].parent!=NS64_ROOT){
   struct ns64_node *q=find(nodes,n,nodes[i].parent);
   if(!q||q->kind!=SD64_DIRECTORY)return NS64_CORRUPT;
  }
 }
 /* Three-color walk with a second parent walk to close each chain.
  * No recursion or path-sized stack; each node marked at most twice. */
 for(size_t i=0;i<n;i++){
  struct ns64_node *q=nodes+i;
  while(q&&q->mark==0){
   q->mark=1;q=q->parent==NS64_ROOT?0:find(nodes,n,q->parent);
  }
  if(q&&q->mark==1)return NS64_CYCLE;
  q=nodes+i;
  while(q&&q->mark==1){
   q->mark=2;q=q->parent==NS64_ROOT?0:find(nodes,n,q->parent);
  }
 }
 *p=(struct ns64_proof){s,nodes,s->epoch,s->token,s->count,true};
 return NS64_OK;
}
enum ns64_status ns64_resolve(struct dc64_state *s,const struct ns64_proof *p,
 const uint8_t *path,size_t n,uint8_t scratch[SD64_PAGE],uint64_t *id,uint8_t *kind){
 if(!live(s,p))return NS64_STALE;
 size_t bytes=(size_t)p->count*sizeof *p->nodes;
 if(!n||!path||path[0]!='/'||
    !separate(path,n,scratch,SD64_PAGE)||!separate(id,sizeof *id,kind,sizeof *kind)||
    !separate(id,sizeof *id,path,n)||!separate(kind,sizeof *kind,path,n)||
    !separate(id,sizeof *id,scratch,SD64_PAGE)||!separate(kind,sizeof *kind,scratch,SD64_PAGE)||
    !separate(id,sizeof *id,s,sizeof *s)||!separate(kind,sizeof *kind,s,sizeof *s)||
    !separate(id,sizeof *id,p,sizeof *p)||!separate(kind,sizeof *kind,p,sizeof *p)||
    !separate(id,sizeof *id,p->nodes,bytes)||!separate(kind,sizeof *kind,p->nodes,bytes)||
    !separate(id,sizeof *id,s->block,sizeof *s->block)||!separate(kind,sizeof *kind,s->block,sizeof *s->block)||
    !separate(scratch,SD64_PAGE,p,sizeof *p)||!separate(scratch,SD64_PAGE,p->nodes,bytes)||
    !separate(scratch,SD64_PAGE,s,sizeof *s)||!separate(scratch,SD64_PAGE,s->block,sizeof *s->block))return NS64_INVALID;
 /* Root-only lookup must also verify a live grant; an ordinal miss checks it. */
 if(!s->block||s->block->busy)return NS64_STALE;
 bool granted=false;
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=&s->block->grants[i];
  if(g->token==s->token&&g->owner==s->owner&&(g->rights&BS_READ)&&g->device<BS_DEVICES&&g->length==s->sectors){
   const struct bs_device *d=&s->block->devices[g->device];
   granted=g->generation&&g->generation==d->generation&&!d->recovery&&g->start<d->backend.sectors&&g->length<=d->backend.sectors-g->start;
  }
 }
 if(!granted)return NS64_STALE;
 if(n>1&&path[n-1]=='/')return NS64_INVALID;
 /* Validate syntax of the complete path before lookup. */
 for(size_t i=1;i<n;){
  struct sd64_key k={.parent=NS64_ROOT};size_t start=i;
  while(i<n&&path[i]!='/'){
   if(i-start==SD64_NAME)return NS64_INVALID;
   k.name[i-start]=path[i];++i;
  }
  k.length=(uint16_t)(i-start);if(!sd64_key_valid(&k))return NS64_INVALID;
  if(i<n)++i;
 }
 uint64_t current=NS64_ROOT;uint8_t type=SD64_DIRECTORY;
 for(size_t i=1;i<n;){
  if(type!=SD64_DIRECTORY)return NS64_NOT_DIRECTORY;
  struct sd64_key k={.parent=current};size_t start=i;
  while(i<n&&path[i]!='/'){k.name[i-start]=path[i];++i;}
  k.length=(uint16_t)(i-start);if(i<n)++i;
  if(!s->count)return NS64_MISSING;
  enum sd64_status rc=sd64_lookup(s->block,s->owner,s->token,&s->root,&k,scratch,&current,&type);
  if(rc!=SD64_OK){
   if(rc==SD64_IO||rc==SD64_CORRUPT)s->valid=false;
   return rc==SD64_MISSING?NS64_MISSING:rc==SD64_IO?NS64_IO:rc==SD64_CORRUPT?NS64_CORRUPT:NS64_INVALID;
  }
  const struct ns64_node *node=find(p->nodes,(size_t)p->count,current);
  if(!node||node->parent!=k.parent||node->kind!=type){s->valid=false;return NS64_CORRUPT;}
 }
 *id=current;*kind=type;return NS64_OK;
}
enum ns64_status ns64_apply(struct dc64_state *s,struct ns64_proof *p,
 struct dc64_workspace *w,struct dc64_workspace *r,const struct dc64_edit *e){
 if(!live(s,p))return NS64_STALE;
 size_t n=(size_t)p->count,bytes=n*sizeof *p->nodes;
 if(!separate(p,sizeof *p,w,sizeof *w)||!separate(p,sizeof *p,r,sizeof *r)||
    !separate(p,sizeof *p,e,sizeof *e)||!separate(p->nodes,bytes,w,sizeof *w)||
    !separate(p->nodes,bytes,r,sizeof *r)||!separate(p->nodes,bytes,e,sizeof *e)||
    !e||e->entry.target<=NS64_ROOT||!sd64_key_valid(&e->entry.key)||
    (e->entry.kind!=SD64_FILE&&e->entry.kind!=SD64_DIRECTORY)||
    (e->op!=DC64_INSERT&&e->op!=DC64_DELETE&&e->op!=DC64_RENAME))return NS64_INVALID;
 const struct ns64_node *node=find(p->nodes,n,e->entry.target);
 if(e->op==DC64_INSERT){if(node)return NS64_CONFLICT;}
 else if(!node||node->parent!=e->entry.key.parent||node->kind!=e->entry.kind)return NS64_STALE;
 if(e->op==DC64_DELETE&&e->entry.kind==SD64_DIRECTORY){
  for(size_t i=0;i<n;i++)if(p->nodes[i].parent==e->entry.target)return NS64_NOT_EMPTY;
 }
 if(e->op!=DC64_DELETE){
  if(e->op==DC64_RENAME&&!sd64_key_valid(&e->destination))return NS64_INVALID;
  uint64_t parent=e->op==DC64_RENAME?e->destination.parent:e->entry.key.parent;
  const struct ns64_node *q=parent==NS64_ROOT?0:find(p->nodes,n,parent);
  if(parent!=NS64_ROOT&&(!q||q->kind!=SD64_DIRECTORY))return NS64_NOT_DIRECTORY;
  for(size_t hops=0;q;hops++){
   if(q->id==e->entry.target)return NS64_CYCLE;
   if(hops>=n)return NS64_CORRUPT;
   q=q->parent==NS64_ROOT?0:find(p->nodes,n,q->parent);
  }
 }
 enum dc64_status rc=dc64_apply(s,w,r,p->epoch,e);
 if(!s->valid||s->epoch!=p->epoch)p->valid=false;
 return convert(rc);
}
