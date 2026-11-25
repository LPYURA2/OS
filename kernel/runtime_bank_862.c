#include <styleos/runtime_bank_862.h>
#define PAGE UINT64_C(4096)
#define IF UINT64_C(0x200)
static bool region(const void *p,size_t n,size_t a){return p&&!((uintptr_t)p%a)&&n<=UINTPTR_MAX-(uintptr_t)p;}
static bool apart(const void *a,size_t n,const void *b,size_t m){return (uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a;}
static bool root(uint64_t r){return r>=PAGE&&r<(UINT64_C(1)<<47)&&!(r&(PAGE-1));}
static bool zero(const void *p,size_t n){const unsigned char *b=p;for(size_t i=0;i<n;i++)if(b[i])return false;return true;}
static bool layout(const struct rt862_bank *b,const struct dpr_service *d,const void *x,size_t n,size_t align,const void *y,size_t m,size_t al){
 if(!region(b,sizeof *b,_Alignof(struct rt862_bank))||!region(d,sizeof *d,_Alignof(struct dpr_service))||!d->started||!region(d->dispatcher.registry,sizeof *d->dispatcher.registry,_Alignof(struct pr_table))||!region(d->windows,sizeof *d->windows,_Alignof(struct ws_service)))return false;
 if((n&&!region(x,n,align))||(m&&!region(y,m,al)))return false;
 const void *ptr[]={b,d,d->dispatcher.registry,d->windows,x,y};const size_t len[]={sizeof *b,sizeof *d,sizeof *d->dispatcher.registry,sizeof *d->windows,n,m};
 for(unsigned i=0;i<6;i++)for(unsigned j=0;j<i;j++)if(len[i]&&len[j]&&!apart(ptr[i],len[i],ptr[j],len[j]))return false;
 return true;
}
static bool context(const struct rt862_bank *b,const struct up_context *c){
 if(c->cs!=b->cs||c->ss!=b->ss||!(c->status&2)||(c->status&~UINT64_C(0x8d7))||c->pc<S846_CODE||c->pc>=S846_CODE+PAGE||c->sp<=S846_STACK||c->sp>S846_STACK+PAGE)return false;
 for(unsigned i=15;i<31;i++)if(c->gpr[i])return false;
 return true;
}
static bool binding(const struct rt862_bank *b,const struct dpr_service *d){
 const struct ps_dispatcher *s=&d->dispatcher;
 if(!root(b->safe)||b->safe!=s->safe_root||(b->cs&7)!=3||(b->ss&7)!=3||b->cs<11||b->ss<11||b->cs==b->ss||s->current< -1||s->current>=PS_MAX_SLOTS||s->next>=PS_MAX_SLOTS)return false;
 if(b->slots[0].pid==b->slots[1].pid||b->slots[0].handle==b->slots[1].handle||b->slots[0].root==b->slots[1].root)return false;
 unsigned seen[PS_MAX_SLOTS]={0};
 for(unsigned i=0;i<PR_MAX_PROCESSES;i++){
  const struct pr_entry *p=&s->registry->processes[i];if(p->state==PR_UNUSED)continue;
  unsigned j=p->pid==b->slots[0].pid?0:p->pid==b->slots[1].pid?1:PS_MAX_SLOTS;
  if(j==PS_MAX_SLOTS||++seen[j]!=1)return false;
 }
 for(unsigned i=0;i<PS_MAX_SLOTS;i++){
  const struct rt862_bank_slot *c=&b->slots[i];const struct ps_slot *slot=&s->slots[i];const struct pr_entry *p=pr_find(s->registry,c->pid);
  if(seen[i]!=1||!c->pid||!c->handle||!root(c->root)||c->root==b->safe||slot->pid!=c->pid||slot->handle!=c->handle||slot->user_root!=c->root||!p||p->type!=PR_NATIVE_USER||p->scheduler_handle!=c->handle||p->page_root!=c->root||((p->state==PR_RUNNING)!=(s->current==(int)i)))return false;
  if(p->state==PR_READY||p->state==PR_RUNNING){if(!context(b,&c->context))return false;}
  else if(p->state==PR_EXITED||p->state==PR_FAULTED){if(!zero(&c->context,sizeof c->context))return false;}
  else return false;
 }
 return true;
}
static bool ticket(const struct rt862_bank *b,const struct dpr_service *d,const struct rt862_selection *t,uint64_t live){
 if(!binding(b,d)||live!=b->safe||t->safe_root!=live||t->slot>=PS_MAX_SLOTS||d->dispatcher.current!=(int)t->slot||!t->dispatch||t->dispatch!=d->dispatcher.dispatches)return false;
 const struct rt862_bank_slot *c=&b->slots[t->slot];return t->pid==c->pid&&t->handle==c->handle&&t->child_root==c->root;
}
bool rt862_bank_init(struct rt862_bank *b,struct dpr_service *d,uint64_t live,uint16_t cs,uint16_t ss){
 if(!layout(b,d,0,0,1,0,0,1)||b->safe||d->dispatcher.current!=-1||d->dispatcher.dispatches||live!=d->dispatcher.safe_root)return false;
 struct rt862_bank v={.safe=live,.cs=cs,.ss=ss};
 for(unsigned i=0;i<PS_MAX_SLOTS;i++){const struct ps_slot *s=&d->dispatcher.slots[i];const struct pr_entry *p=pr_find(d->dispatcher.registry,s->pid);if(!p||p->state!=PR_READY)return false;v.slots[i]=(struct rt862_bank_slot){.pid=s->pid,.handle=s->handle,.root=s->user_root,.context={.pc=S846_CODE,.sp=S846_STACK+PAGE,.status=2,.cs=cs,.ss=ss}};}
 if(!binding(&v,d))return false;
 *b=v;return true;
}
bool rt862_bank_enter(const struct rt862_bank *b,const struct dpr_service *d,const struct rt862_selection *t,uint64_t live,struct up_context *out){
 if(!layout(b,d,t,sizeof *t,_Alignof(struct rt862_selection),out,sizeof *out,_Alignof(struct up_context))||!ticket(b,d,t,live))return false;
 *out=b->slots[t->slot].context;out->status|=IF;return true;
}
enum ps_status rt862_bank_tick(struct rt862_bank *b,struct dpr_service *d,const struct rt862_selection *t,uint64_t live,const struct rt860_tick *r){
 if(!layout(b,d,t,sizeof *t,_Alignof(struct rt862_selection),r,sizeof *r,_Alignof(struct rt860_tick))||!ticket(b,d,t,live)||r->slot!=t->slot||r->dispatch!=t->dispatch||r->pid!=t->pid||r->handle!=t->handle||r->safe_root!=live||r->child_root!=t->child_root)return PS_EVIDENCE;
 struct rt860_bounds bounds={{S846_CODE,S846_CODE+PAGE},{S846_STACK,S846_STACK+PAGE},{S846_IST,S846_IST+PAGE},b->cs,b->ss};struct s847_raw saved;
 enum ps_status status=dpr_tick_860(d,&bounds,r,&saved);if(status!=PS_OK)return status;
 struct up_context c={.pc=saved.rip,.sp=saved.rsp,.cs=saved.cs,.ss=saved.ss,.status=saved.rflags&~IF};for(unsigned i=0;i<15;i++)c.gpr[i]=saved.gpr[i];
 b->slots[t->slot].context=c;return PS_OK;
}
enum ps_status rt862_bank_finish(struct rt862_bank *b,struct dpr_service *d,const struct rt862_selection *t,uint64_t live,uint64_t safe_flags,const struct s847_raw *f,uint64_t va,uint64_t cr2){
 if(!layout(b,d,t,sizeof *t,_Alignof(struct rt862_selection),f,sizeof *f,_Alignof(struct s847_raw))||!ticket(b,d,t,live)||(safe_flags&~UINT64_C(0x8d7))||!(safe_flags&2)||(f->rflags&0x202)!=0x202||f->rsp<=S846_STACK)return PS_EVIDENCE;
 struct s847_raw normalized=*f;normalized.rflags&=~IF;struct up_context decoded;struct up_event event;
 if(!s847_decode(&normalized,va,cr2,t->child_root,b->cs,b->ss,&decoded,&event))return PS_EVIDENCE;
 enum ps_event end;
 if(event.kind==UP_FAULT)end=PS_FAULT;
 else if(event.vector==128&&f->gpr[0]==S847_CALL_EXIT&&!cr2)end=PS_EXIT;
 else return PS_EVIDENCE;
 enum ps_status status=dpr_observe(d,t->slot,t->handle,t->child_root,live,end);if(status!=PS_OK)return status;
 volatile unsigned char *p=(void *)&b->slots[t->slot].context;for(size_t i=0;i<sizeof(struct up_context);i++)p[i]=0;
 return PS_OK;
}
