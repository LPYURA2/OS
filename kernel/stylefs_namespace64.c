
#include <styleos/stylefs_dircommit64.h>
static bool disjoint(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool workspace_ok(struct dc64_state *s,struct dc64_edit_workspace *w){
 return disjoint(s,sizeof *s,w,sizeof *w)&&s->valid&&s->block&&s->epoch&&s->active<2&&
 disjoint(s,sizeof *s,s->block,sizeof *s->block)&&disjoint(w,sizeof *w,s->block,sizeof *s->block);
}
static enum dc64_status leaf(struct dc64_state *s,struct dc64_edit_workspace *w,uint64_t page,unsigned *n){
 if(!s->count||page>(s->count-1)/SD64_FANOUT||
    s->root.region_sectors<8||page>(s->root.region_sectors-8)/8||
    s->root.region_start>UINT64_MAX-s->root.region_sectors)return DC64_INVALID;
 uint64_t at=s->root.region_start+page*8;
 for(unsigned i=0;i<8;i++)if(bs_read_sector(s->block,s->owner,s->token,at+i,w->page+i*512)!=BS_OK)return DC64_IO;
 uint64_t remain=s->count-page*SD64_FANOUT;
 unsigned expected=remain<SD64_FANOUT?(unsigned)remain:SD64_FANOUT;
 if(!sd64_decode_page(w->page,s->epoch,0,w->records,n)||*n!=expected)return DC64_CORRUPT;
 return DC64_OK;
}
enum dc64_status dc64_entry_at(struct dc64_state *s,uint64_t ordinal,struct dc64_edit_workspace *w,struct sd64_record *out){
 if(!workspace_ok(s,w)||ordinal>=s->count||!disjoint(out,sizeof *out,s,sizeof *s)||
    !disjoint(out,sizeof *out,w,sizeof *w)||!disjoint(out,sizeof *out,s->block,sizeof *s->block))return DC64_INVALID;
 unsigned n=0;enum dc64_status rc=leaf(s,w,ordinal/SD64_FANOUT,&n);
 if(rc==DC64_OK)*out=w->records[ordinal%SD64_FANOUT];
 else s->valid=false;
 return rc;
}
static enum dc64_status find(struct dc64_state *s,struct dc64_edit_workspace *w,const struct sd64_key *key,bool *exists,uint64_t *object,uint8_t *kind){
 *exists=false;
 if(!s->count)return DC64_OK;
 enum sd64_status rc=sd64_lookup(s->block,s->owner,s->token,&s->root,key,w->page,object,kind);
 if(rc==SD64_OK){*exists=true;return DC64_OK;}
 if(rc==SD64_MISSING)return DC64_OK;
 s->valid=false;
 return rc==SD64_IO?DC64_IO:rc==SD64_CORRUPT?DC64_CORRUPT:DC64_INVALID;
}
struct merge {
 struct dc64_state *s;struct dc64_edit_workspace *w;
 struct sd64_record add,remove;
 uint64_t cursor,cached,produced;
 bool has_add,has_remove,added,removed;
 enum dc64_status status;
};
static bool advance(void *ptr,uint64_t index,struct sd64_record *out){
 struct merge *m=ptr;
 if(index!=m->produced){m->status=DC64_INVALID;return false;}
 while(m->cursor<m->s->count){
  uint64_t page=m->cursor/SD64_FANOUT;
  if(page!=m->cached){
   unsigned n=0;m->status=leaf(m->s,m->w,page,&n);
   if(m->status!=DC64_OK)return false;
   m->cached=page;
  }
  const struct sd64_record *r=&m->w->records[m->cursor%SD64_FANOUT];
  if(m->has_remove&&sd64_compare(&r->key,&m->remove.key)==0){
   if(m->removed||r->target!=m->remove.target||r->kind!=m->remove.kind){m->status=DC64_CORRUPT;return false;}
   m->removed=true;m->cursor++;continue;
  }
  if(m->has_add&&!m->added&&sd64_compare(&m->add.key,&r->key)<0){
   *out=m->add;m->added=true;m->produced++;return true;
  }
  *out=*r;m->cursor++;m->produced++;return true;
 }
 if(m->has_add&&!m->added){*out=m->add;m->added=true;m->produced++;return true;}
 m->status=DC64_CORRUPT;return false;
}
static enum dc64_status apply(struct dc64_state *s,struct dc64_edit_workspace *w,const struct dc64_edit *e,bool hierarchy){
 if(!workspace_ok(s,w)||!disjoint(e,sizeof *e,s,sizeof *s)||
    !disjoint(e,sizeof *e,w,sizeof *w)||!disjoint(e,sizeof *e,s->block,sizeof *s->block)||
    !sd64_key_valid(&e->entry.key)||!e->entry.target||
    (e->entry.kind!=SD64_FILE&&e->entry.kind!=SD64_DIRECTORY)||
    (e->op!=DC64_CREATE&&e->op!=DC64_REMOVE&&e->op!=DC64_RENAME)||
    (!hierarchy&&e->op!=DC64_CREATE&&e->entry.kind!=SD64_FILE))return DC64_INVALID;
 /* Validate live snapshot before using cached metadata for any edit. */
 struct dc64_state live;
 enum dc64_status rc=dc64_mount(&live,s->block,s->owner,s->token,s->sectors,&w->publish);
 if(rc!=DC64_OK){s->valid=false;return rc;}
 if(live.epoch!=s->epoch||live.active!=s->active||live.count!=s->count){s->valid=false;return DC64_INVALID;}
 *s=live;
 bool exists;uint64_t object=0;uint8_t kind=0;
 rc=find(s,w,&e->entry.key,&exists,&object,&kind);
 if(rc!=DC64_OK)return rc;
 if(e->op==DC64_CREATE?exists:(!exists||object!=e->entry.target||kind!=e->entry.kind))return DC64_INVALID;
 struct merge m={.s=s,.w=w,.add=e->entry,.remove=e->entry,.cached=UINT64_MAX,
   .has_add=e->op!=DC64_REMOVE,.has_remove=e->op!=DC64_CREATE,.status=DC64_OK};
 uint64_t count=s->count;
 if(e->op==DC64_CREATE){if(count==UINT64_MAX)return DC64_FULL;count++;}
 else if(e->op==DC64_REMOVE)count--;
 else{
  if(!sd64_key_valid(&e->destination)||sd64_compare(&e->entry.key,&e->destination)==0)return DC64_INVALID;
  rc=find(s,w,&e->destination,&exists,&object,&kind);
  if(rc!=DC64_OK)return rc;
  if(exists)return DC64_INVALID;
  m.add.key=e->destination;
 }
 /* The preflight lookup plus fully validated immutable snapshot guarantees
  * the skipped key exists even when it is the final entry (or only entry).
  * Publication never reads from the inactive bank as source. */
 rc=dc64_publish(s,&w->publish,count,advance,&m);
 if(rc==DC64_CANCELLED&&m.status!=DC64_OK){s->valid=false;return m.status;}
 return rc;
}

enum dc64_status dc64_apply(struct dc64_state *s,struct dc64_edit_workspace *w,const struct dc64_edit *e){
 return apply(s,w,e,false);
}
static bool graph_workspace(struct dc64_state *s,struct dc64_edit_workspace *w,struct dc64_node *nodes,size_t cap){
 return workspace_ok(s,w)&&cap&&cap<=SIZE_MAX/sizeof *nodes&&
 disjoint(nodes,cap*sizeof *nodes,s,sizeof *s)&&
 disjoint(nodes,cap*sizeof *nodes,w,sizeof *w)&&
 disjoint(nodes,cap*sizeof *nodes,s->block,sizeof *s->block);
}
static void swap_node(struct dc64_node *a,struct dc64_node *b){struct dc64_node t=*a;*a=*b;*b=t;}
static void sift(struct dc64_node *a,size_t n,size_t i){
 while(i<n/2){
  size_t child=i*2+1;
  if(child+1<n&&a[child].id<a[child+1].id)child++;
  if(a[i].id>=a[child].id)break;
  swap_node(a+i,a+child);i=child;
 }
}
static void sort_nodes(struct dc64_node *a,size_t n){
 for(size_t i=n/2;i;i--)sift(a,n,i-1);
 for(size_t i=n;i>1;i--){swap_node(a,a+i-1);sift(a,i-1,0);}
}
static size_t node_index(const struct dc64_node *a,size_t n,uint64_t id){
 size_t lo=0,hi=n;
 while(lo<hi){size_t m=lo+(hi-lo)/2;if(a[m].id<id)lo=m+1;else hi=m;}
 return lo<n&&a[lo].id==id?lo:n;
}
static bool graph_valid(struct dc64_node *a,size_t n){
 sort_nodes(a,n);
 for(size_t i=0;i<n;i++)
  if(a[i].id<=1||!a[i].parent||(i&&a[i-1].id==a[i].id)||
    (a[i].kind!=SD64_FILE&&a[i].kind!=SD64_DIRECTORY))return false;
 for(size_t i=0;i<n;i++){
  uint64_t parent=a[i].parent;unsigned depth=1;
  while(parent!=1){
   if(depth>=DC64_PATH_MAX)return false;
   size_t p=node_index(a,n,parent);
   if(p==n||a[p].kind!=SD64_DIRECTORY)return false;
   parent=a[p].parent;depth++;
  }
 }
 return true;
}
static enum dc64_status graph_collect(struct dc64_state *s,struct dc64_edit_workspace *w,struct dc64_node *a,size_t cap){
 struct dc64_state live;
 enum dc64_status rc=dc64_mount(&live,s->block,s->owner,s->token,s->sectors,&w->publish);
 if(rc!=DC64_OK){s->valid=false;return rc;}
 if(live.epoch!=s->epoch||live.active!=s->active||live.count!=s->count){s->valid=false;return DC64_INVALID;}
 *s=live;
 if(s->count>cap)return DC64_FULL;
 for(uint64_t i=0;i<s->count;){
  unsigned n=0;rc=leaf(s,w,i/SD64_FANOUT,&n);
  if(rc!=DC64_OK){s->valid=false;return rc;}
  for(unsigned j=0;j<n;j++,i++){
   const struct sd64_record *r=&w->records[j];
   a[(size_t)i]=(struct dc64_node){r->target,r->key.parent,r->kind};
  }
 }
 if(!graph_valid(a,(size_t)s->count))return DC64_CORRUPT;
 return DC64_OK;
}
enum dc64_status dc64_hierarchy_check(struct dc64_state *s,struct dc64_edit_workspace *w,struct dc64_node *a,size_t cap){
 if(!graph_workspace(s,w,a,cap))return DC64_INVALID;
 return graph_collect(s,w,a,cap);
}
enum dc64_status dc64_hierarchy_apply(struct dc64_state *s,struct dc64_edit_workspace *w,struct dc64_node *a,size_t cap,const struct dc64_edit *e){
 if(!graph_workspace(s,w,a,cap)||!disjoint(e,sizeof *e,s,sizeof *s)||
    !disjoint(e,sizeof *e,w,sizeof *w)||!disjoint(e,sizeof *e,s->block,sizeof *s->block)||
    !disjoint(e,sizeof *e,a,cap*sizeof *a)||!sd64_key_valid(&e->entry.key)||
    e->entry.target<=1||(e->entry.kind!=SD64_FILE&&e->entry.kind!=SD64_DIRECTORY)||
    (e->op!=DC64_CREATE&&e->op!=DC64_REMOVE&&e->op!=DC64_RENAME)||
    (e->op==DC64_RENAME&&!sd64_key_valid(&e->destination)))return DC64_INVALID;
 enum dc64_status rc=graph_collect(s,w,a,cap);
 if(rc!=DC64_OK)return rc;
 size_t n=(size_t)s->count,p=node_index(a,n,e->entry.target);
 if(e->op==DC64_CREATE){
  if(p!=n)return DC64_INVALID;
  if(n==cap)return DC64_FULL;
  a[n++]=(struct dc64_node){e->entry.target,e->entry.key.parent,e->entry.kind};
 }else{
  if(p==n||a[p].parent!=e->entry.key.parent||a[p].kind!=e->entry.kind)return DC64_INVALID;
  if(e->op==DC64_RENAME)a[p].parent=e->destination.parent;
  else {for(size_t i=p+1;i<n;i++)a[i-1]=a[i];n--;}
 }
 /* Removing a parent with children leaves an orphan; moving below
  * descendants creates a cycle. Validate the entire proposed graph
  * (including all descendants' new depths) before writing any sector. */
 if(!graph_valid(a,n))return DC64_INVALID;
 return apply(s,w,e,true);
}
enum dc64_status dc64_resolve(struct dc64_state *s,struct dc64_edit_workspace *w,struct dc64_node *a,size_t cap,
 const struct dc64_path_part *parts,unsigned count,uint64_t *object,uint8_t *kind){
 if(!graph_workspace(s,w,a,cap)||count>DC64_PATH_MAX||
    !disjoint(object,sizeof *object,kind,sizeof *kind)||
    !disjoint(object,sizeof *object,s,sizeof *s)||!disjoint(kind,sizeof *kind,s,sizeof *s)||
    !disjoint(object,sizeof *object,w,sizeof *w)||!disjoint(kind,sizeof *kind,w,sizeof *w)||
    !disjoint(object,sizeof *object,a,cap*sizeof *a)||!disjoint(kind,sizeof *kind,a,cap*sizeof *a)||
    !disjoint(object,sizeof *object,s->block,sizeof *s->block)||
    !disjoint(kind,sizeof *kind,s->block,sizeof *s->block))return DC64_INVALID;
 if(count&&(!disjoint(parts,count*sizeof *parts,s,sizeof *s)||
    !disjoint(parts,count*sizeof *parts,w,sizeof *w)||
    !disjoint(parts,count*sizeof *parts,a,cap*sizeof *a)||
    !disjoint(parts,count*sizeof *parts,s->block,sizeof *s->block)||
    !disjoint(parts,count*sizeof *parts,object,sizeof *object)||
    !disjoint(parts,count*sizeof *parts,kind,sizeof *kind)))return DC64_INVALID;
 enum dc64_status rc=graph_collect(s,w,a,cap);
 if(rc!=DC64_OK)return rc;
 uint64_t id=1;uint8_t type=SD64_DIRECTORY;
 for(unsigned i=0;i<count;i++){
  if(type!=SD64_DIRECTORY)return DC64_INVALID;
  struct sd64_key k={.parent=id,.length=parts[i].length};
  for(unsigned j=0;j<SD64_NAME;j++)k.name[j]=parts[i].name[j];
  if(!sd64_key_valid(&k))return DC64_INVALID;
  bool exists=false;rc=find(s,w,&k,&exists,&id,&type);
  if(rc!=DC64_OK)return rc;
  if(!exists)return DC64_INVALID;
 }
 *object=id;*kind=type;return DC64_OK;
}
