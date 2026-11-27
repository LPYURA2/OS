#include <styleos/stylefs_process_file64.h>
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool valid(const struct pf64_service *s){
 return s&&sep(s,sizeof *s,s->processes,sizeof *s->processes)&&
 sep(s,sizeof *s,s->broker,sizeof *s->broker)&&
 sep(s->processes,sizeof *s->processes,s->broker,sizeof *s->broker);
}
static bool ext(const struct pf64_service *s,const void *p,size_t n){
 return sep(s,sizeof *s,p,n)&&sep(s->processes,sizeof *s->processes,p,n)&&
 sep(s->broker,sizeof *s->broker,p,n);
}
static bool authenticated(const struct pf64_service *s,const struct pf64_identity *a,bool running){
 if(!a||!a->pid||!a->handle||!a->root||(a->root&4095u))return false;
 const struct pr_entry *p=pr_find(s->processes,a->pid);
 return p&&p->type==PR_NATIVE_USER&&p->scheduler_handle==a->handle&&p->page_root==a->root&&
 (running?p->state==PR_RUNNING:pr_is_live(s->processes,a->pid));
}
static bool storage(const struct pf64_service *s,struct vm64_mount *m,struct wv64_workspace *w){
 return ext(s,m,sizeof *m)&&ext(s,w,sizeof *w)&&m->block&&
 ext(s,m->block,sizeof *m->block)&&m->volume.slots<=SIZE_MAX/sizeof *m->volume.nodes&&
 ext(s,m->volume.nodes,m->volume.slots*sizeof *m->volume.nodes);
}
enum wv64_status pf64_sweep(const struct pf64_service *s){
 if(!valid(s))return WV64_INVALID;
 for(unsigned i=0;i<FBK64_SLOTS;i++){
  const struct fbk64_slot *v=s->broker->slots+i;
  if(!v->token)continue;
  uint64_t principal=v->principal;
  const struct pr_entry *p=principal<=UINT32_MAX?pr_find(s->processes,(uint32_t)principal):0;
  if(!p||p->type!=PR_NATIVE_USER||!pr_is_live(s->processes,p->pid)){
   if(fbk64_revoke(s->broker,principal,v->token)!=WV64_OK)return WV64_CORRUPT;
  }
 }
 return WV64_OK;
}
static enum wv64_status issue(const struct pf64_service *s,struct vm64_mount *m,struct wv64_workspace *w,
 const struct pf64_identity *a,const uint8_t *path,size_t n,uint64_t *token,bool save){
 if(!valid(s)||!storage(s,m,w)||!ext(s,a,sizeof *a)||!ext(s,path,n)||!ext(s,token,sizeof *token)||
    !sep(a,sizeof *a,token,sizeof *token))return WV64_INVALID;
 if(!authenticated(s,a,false))return WV64_LOCKED;
 enum wv64_status rc=pf64_sweep(s);if(rc!=WV64_OK)return rc;
 return save?fbk64_issue_save(s->broker,m,w,a->pid,path,n,token):fbk64_issue(s->broker,m,w,a->pid,path,n,token);
}
enum wv64_status pf64_read(const struct pf64_service *s,struct vm64_mount *m,struct wv64_workspace *w,
 const struct pf64_identity *a,uint64_t token,uint64_t off,uint8_t *out,size_t n,size_t *done){
 if(!valid(s)||!storage(s,m,w)||!ext(s,a,sizeof *a)||!ext(s,out,n)||!ext(s,done,sizeof *done)||
    !sep(a,sizeof *a,out,n)||!sep(a,sizeof *a,done,sizeof *done))return WV64_INVALID;
 if(!authenticated(s,a,true))return WV64_LOCKED;
 return fbk64_read(s->broker,m,w,a->pid,token,off,out,n,done);
}

enum wv64_status pf64_issue(const struct pf64_service *s,struct vm64_mount *m,struct wv64_workspace *w,
 const struct pf64_identity *a,const uint8_t *path,size_t n,uint64_t *token){
 return issue(s,m,w,a,path,n,token,false);
}
enum wv64_status pf64_issue_save(const struct pf64_service *s,struct vm64_mount *m,struct wv64_workspace *w,
 const struct pf64_identity *a,const uint8_t *path,size_t n,uint64_t *token){
 return issue(s,m,w,a,path,n,token,true);
}
enum wv64_status pf64_save(const struct pf64_service *s,struct vm64_mount *m,struct wv64_workspace *w,
 struct ow64_inventory inv[2],const struct pf64_identity *a,uint64_t token,const uint8_t *p,size_t n){
 if(!valid(s)||!storage(s,m,w)||!ext(s,a,sizeof *a)||
    !ext(s,inv,2*sizeof *inv)||!sep(a,sizeof *a,inv,2*sizeof *inv)||
    (n&&(!ext(s,p,n)||!sep(a,sizeof *a,p,n))))return WV64_INVALID;
 if(!authenticated(s,a,true))return WV64_LOCKED;
 for(unsigned i=0;i<2;i++){
  if(!inv[i].slots||inv[i].slots>SIZE_MAX/sizeof *inv[i].entries||
     inv[i].slots>SIZE_MAX/sizeof *inv[i].runs)return WV64_INVALID;
  size_t e=inv[i].slots*sizeof *inv[i].entries,r=inv[i].slots*sizeof *inv[i].runs;
  if(!ext(s,inv[i].entries,e)||!ext(s,inv[i].runs,r)||
     !sep(a,sizeof *a,inv[i].entries,e)||!sep(a,sizeof *a,inv[i].runs,r))return WV64_INVALID;
 }
 return fbk64_save(s->broker,m,w,inv,a->pid,token,p,n);
}
