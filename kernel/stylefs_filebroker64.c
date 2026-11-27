#include <styleos/stylefs_filebroker64.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool ready(const struct vm64_mount *m){
 return m&&m->active&&m->block&&m->volume.ready&&!m->volume.busy&&
 !m->block->busy&&m->volume.block==m->block&&m->grants.receipt&&
 m->volume.layout.receipt==m->grants.receipt&&m->volume.nodes&&
 m->volume.slots&&m->volume.slots<=SIZE_MAX/sizeof *m->volume.nodes;
}
static bool buffers(struct fbk64_broker *b,struct vm64_mount *m,struct wv64_workspace *w){
 const void *ptr[]={b,m,w,m->block,m->volume.nodes};
 const size_t size[]={sizeof *b,sizeof *m,sizeof *w,sizeof *m->block,m->volume.slots*sizeof *m->volume.nodes};
 for(unsigned i=0;i<5;i++)for(unsigned j=0;j<i;j++)if(!sep(ptr[i],size[i],ptr[j],size[j]))return false;
 return true;
}
static bool external(struct fbk64_broker *b,struct vm64_mount *m,struct wv64_workspace *w,const void *p,size_t n){
 return sep(p,n,b,sizeof *b)&&sep(p,n,m,sizeof *m)&&sep(p,n,w,sizeof *w)&&
 sep(p,n,m->block,sizeof *m->block)&&sep(p,n,m->volume.nodes,m->volume.slots*sizeof *m->volume.nodes);
}
static bool current(const struct fbk64_slot *s,const struct vm64_mount *m){
 return s->mount==m&&s->receipt==m->grants.receipt&&s->sequence==m->volume.sequence;
}
static struct fbk64_slot *lookup(struct fbk64_broker *b,uint64_t token,uint64_t principal){
 if(!b||!token||!principal)return 0;
 for(unsigned i=0;i<FBK64_SLOTS;i++)if(b->slots[i].token==token&&b->slots[i].principal==principal)return b->slots+i;
 return 0;
}
static enum wv64_status issue(struct fbk64_broker *b,struct vm64_mount *m,struct wv64_workspace *w,
 uint64_t principal,const uint8_t *path,size_t n,uint64_t *token,bool save){
 if(!ready(m)||(save&&!m->volume.write))return WV64_LOCKED;
 if(!principal||!n||n>FBK64_PATH_MAX||!buffers(b,m,w)||
    !external(b,m,w,path,n)||!external(b,m,w,token,sizeof *token)||!sep(path,n,token,sizeof *token))return WV64_INVALID;
 if(b->serial==UINT64_MAX)return WV64_FULL;
 struct fbk64_slot *slot=0;
 for(unsigned i=0;i<FBK64_SLOTS;i++){
  struct fbk64_slot *s=b->slots+i;
  if(s->token&&s->mount==m&&!current(s,m))*s=(struct fbk64_slot){0};
  if(!s->token&&!slot)slot=s;
 }
 if(!slot)return WV64_FULL;
 struct sfs64_file file;
 enum wv64_status rc=wv64_stat(&m->volume,w,path,n,&file);
 if(rc!=WV64_OK)return rc;
 slot->save=save;slot->mount=m;slot->principal=principal;slot->receipt=m->grants.receipt;
 slot->sequence=m->volume.sequence;slot->id=file.id;slot->revision=file.revision;slot->length=n;
 for(size_t i=0;i<n;i++)slot->path[i]=path[i];
 slot->token=++b->serial;*token=slot->token;return WV64_OK;
}
enum wv64_status fbk64_read(struct fbk64_broker *b,struct vm64_mount *m,struct wv64_workspace *w,
 uint64_t principal,uint64_t token,uint64_t offset,uint8_t *out,size_t n,size_t *completed){
 if(!ready(m))return WV64_LOCKED;
 if(!buffers(b,m,w)||!external(b,m,w,out,n)||!external(b,m,w,completed,sizeof *completed)||
    !sep(out,n,completed,sizeof *completed)||n>SFS64_WINDOW_BYTES)return WV64_INVALID;
 struct fbk64_slot *slot=lookup(b,token,principal);
 if(!slot||!current(slot,m))return WV64_LOCKED;
 if(!slot->length||slot->length>FBK64_PATH_MAX)return WV64_CORRUPT;
 return wv64_read(&m->volume,w,slot->path,slot->length,slot->id,slot->revision,offset,out,n,completed);
}
enum wv64_status fbk64_revoke(struct fbk64_broker *b,uint64_t principal,uint64_t token){
 struct fbk64_slot *slot=lookup(b,token,principal);
 if(!slot)return WV64_LOCKED;
 *slot=(struct fbk64_slot){0};return WV64_OK;
}
enum wv64_status fbk64_revoke_principal(struct fbk64_broker *b,uint64_t principal){
 if(!b||!principal)return WV64_INVALID;
 for(unsigned i=0;i<FBK64_SLOTS;i++)if(b->slots[i].principal==principal)b->slots[i]=(struct fbk64_slot){0};
 return WV64_OK;
}

enum wv64_status fbk64_issue(struct fbk64_broker *b,struct vm64_mount *m,struct wv64_workspace *w,
 uint64_t principal,const uint8_t *path,size_t n,uint64_t *token){
 return issue(b,m,w,principal,path,n,token,false);
}
enum wv64_status fbk64_issue_save(struct fbk64_broker *b,struct vm64_mount *m,struct wv64_workspace *w,
 uint64_t principal,const uint8_t *path,size_t n,uint64_t *token){
 return issue(b,m,w,principal,path,n,token,true);
}
struct save_buffer {const uint8_t *p;size_t n;};
static bool buffer_source(void *ctx,uint64_t off,uint8_t *out,size_t n){
 const struct save_buffer *s=ctx;
 if(off>s->n||n>s->n-(size_t)off)return false;
 for(size_t i=0;i<n;i++)out[i]=s->p[(size_t)off+i];
 return true;
}
enum wv64_status fbk64_save(struct fbk64_broker *b,struct vm64_mount *m,struct wv64_workspace *w,
 struct ow64_inventory inv[2],uint64_t principal,uint64_t token,const uint8_t *p,size_t n){
 if(!ready(m)||!m->volume.write)return WV64_LOCKED;
 if(!buffers(b,m,w)||n>SFS64_WINDOW_BYTES||(!n&&p)||
    (n&&!external(b,m,w,p,n))||!external(b,m,w,inv,2*sizeof *inv)||
    (n&&!sep(p,n,inv,2*sizeof *inv)))return WV64_INVALID;
 struct fbk64_slot *slot=lookup(b,token,principal);
 if(!slot||!current(slot,m)||!slot->save)return WV64_LOCKED;
 if(!slot->length||slot->length>FBK64_PATH_MAX)return WV64_CORRUPT;
 for(unsigned i=0;i<2;i++){
  if(!inv[i].slots||inv[i].slots>SIZE_MAX/sizeof *inv[i].entries||
     inv[i].slots>SIZE_MAX/sizeof *inv[i].runs)return WV64_INVALID;
  size_t e=inv[i].slots*sizeof *inv[i].entries,r=inv[i].slots*sizeof *inv[i].runs;
  if(!external(b,m,w,inv[i].entries,e)||!external(b,m,w,inv[i].runs,r)||
     (n&&(!sep(p,n,inv[i].entries,e)||!sep(p,n,inv[i].runs,r))))return WV64_INVALID;
 }
 /* Local selected path and source descriptor are immutable for the entire
  * serialized operation. Underlying volume validates all inventory overlaps. */
 struct fbk64_slot selected=*slot;
 struct save_buffer source={p,n};
 return wv64_replace(&m->volume,w,inv,selected.path,selected.length,selected.id,
  selected.revision,n,n?buffer_source:0,&source);
}
