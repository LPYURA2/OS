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
 for(unsigned i=0;i<PS_MAX_SLOTS;i++)for(unsigned j=0;j<i;j++){
  const struct rt862_bank_slot *x=&b->slots[i],*y=&b->slots[j];
  if(x->pid&&y->pid&&(x->pid==y->pid||x->handle==y->handle||x->root==y->root))return false;
 }
 unsigned seen[PS_MAX_SLOTS]={0};
 for(unsigned i=0;i<PR_MAX_PROCESSES;i++){
  const struct pr_entry *p=&s->registry->processes[i];if(p->state==PR_UNUSED)continue;
  unsigned j=0;while(j<PS_MAX_SLOTS&&(!b->slots[j].pid||p->pid!=b->slots[j].pid))j++;
  if(j==PS_MAX_SLOTS||++seen[j]!=1)return false;
 }
 for(unsigned i=0;i<PS_MAX_SLOTS;i++){
  const struct rt862_bank_slot *c=&b->slots[i];const struct ps_slot *slot=&s->slots[i];const struct pr_entry *p=pr_find(s->registry,c->pid);
  if(!c->pid){if(seen[i]||c->handle||c->root||!zero(&c->context,sizeof c->context)||slot->pid||slot->handle||slot->user_root||s->current==(int)i)return false;continue;}
  if(seen[i]!=1||!c->pid||!c->handle||!root(c->root)||c->root==b->safe||slot->pid!=c->pid||slot->handle!=c->handle||slot->user_root!=c->root||!p||p->type!=PR_NATIVE_USER||p->scheduler_handle!=c->handle||p->page_root!=c->root||((p->state==PR_RUNNING)!=(s->current==(int)i)))return false;
  if(p->state==PR_READY||p->state==PR_RUNNING){if(!context(b,&c->context))return false;}
  else if(p->state==PR_EXITED||p->state==PR_FAULTED){if(!zero(&c->context,sizeof c->context))return false;}
  else return false;
 }
 return true;
}
static bool ticket(const struct rt862_bank *b,const struct dpr_service *d,const struct rt862_selection *t,uint64_t live){
 if(!binding(b,d)||live!=b->safe||t->safe_root!=live||t->slot>=PS_MAX_SLOTS||d->dispatcher.current!=(int)t->slot||!t->dispatch||t->dispatch!=d->dispatcher.dispatches)return false;
 const struct rt862_bank_slot *c=&b->slots[t->slot];return c->pid&&t->pid==c->pid&&t->handle==c->handle&&t->child_root==c->root;
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

bool rt862_bank_open(struct rt862_bank *b,struct dpr_service *d,uint64_t live,uint16_t cs,uint16_t ss){
 if(!layout(b,d,0,0,1,0,0,1)||!zero(b,sizeof *b)||d->dispatcher.current!=-1||live!=d->dispatcher.safe_root||ws_snapshot(d->windows,0,0))return false;
 struct rt862_bank v={.safe=live,.cs=cs,.ss=ss};
 if(!binding(&v,d))return false;
 *b=v;return true;
}
enum ps_status rt862_bank_admit(struct rt862_bank *b,struct dpr_service *d,uint64_t live,unsigned slot,uint64_t handle,uint64_t child,const char *name,uint32_t *pid){
 if(!layout(b,d,pid,sizeof *pid,_Alignof(uint32_t),0,0,1)||!name||!binding(b,d)||live!=b->safe||d->dispatcher.current!=-1||slot>=PS_MAX_SLOTS||!handle||!root(child)||child==live)return PS_EVIDENCE;
 if(b->slots[slot].pid)return PS_STATE;
 /* Copy the trusted, bounded name before mutating the registry. */
 char label[PR_LABEL_BYTES]={0};unsigned i=0;
 for(;i<sizeof label;i++){label[i]=name[i];if(!label[i])break;}
 if(i==sizeof label||!i)return PS_ARGUMENT;
 uint32_t assigned=0;enum ps_status status=dpr_admit(d,slot,handle,child,0,label,&assigned);
 if(status!=PS_OK)return status;
 b->slots[slot]=(struct rt862_bank_slot){.pid=assigned,.handle=handle,.root=child,.context={.pc=S846_CODE,.sp=S846_STACK+PAGE,.status=2,.cs=b->cs,.ss=b->ss}};
 *pid=assigned;return PS_OK;
}
enum ps_status rt862_bank_release(struct rt862_bank *b,struct dpr_service *d,uint64_t live,unsigned slot,uint32_t pid,uint64_t handle,uint64_t child){
 if(!layout(b,d,0,0,1,0,0,1)||!binding(b,d)||live!=b->safe||d->dispatcher.current!=-1||slot>=PS_MAX_SLOTS||!pid)return PS_EVIDENCE;
 struct rt862_bank_slot *c=&b->slots[slot];
 if(c->pid!=pid||c->handle!=handle||c->root!=child||!zero(&c->context,sizeof c->context))return PS_EVIDENCE;
 for(unsigned i=0;i<WS_MAX_WINDOWS;i++)if(d->windows->windows[i].id&&d->windows->windows[i].owner_pid==pid)return PS_STATE;
 enum ps_status status=dpr_release(d,slot,handle);if(status!=PS_OK)return status;
 volatile unsigned char *p=(void *)c;for(size_t i=0;i<sizeof *c;i++)p[i]=0;
 return PS_OK;
}

#ifdef STYLEOS_TEST_RUNTIME_SYSCALL_863
#include <styleos/user_copy_772.h>
enum ps_status rt863_bank_admit_initial(struct rt862_bank *b,struct dpr_service *d,
 uint64_t live,unsigned slot,uint64_t handle,uint64_t child,const char *name,
 const struct up_context *initial,uint32_t *pid)
{
 /* Validate all independently owned objects before reading initial or
  * changing the registry. Copy once under the serialized supervisor. */
 if(!layout(b,d,initial,sizeof *initial,_Alignof(struct up_context),
            pid,sizeof *pid,_Alignof(uint32_t))||
    !binding(b,d)||live!=b->safe||d->dispatcher.current!=-1||
    slot>=PS_MAX_SLOTS)return PS_EVIDENCE;
 const struct up_context proposed=*initial;
 if(!context(b,&proposed)||proposed.status!=2||(proposed.sp&15u))
    return PS_EVIDENCE;
 uint32_t assigned=0;
 enum ps_status status=rt862_bank_admit(b,d,live,slot,handle,child,name,&assigned);
 if(status!=PS_OK)return status;
 /* No further fallible operation and no dispatch can interleave here.
  * The sole published context is the validated loader-supplied state. */
 b->slots[slot].context=proposed;
 *pid=assigned;
 return PS_OK;
}

#if defined(STYLEOS_INTEGRATED_RUNTIME_864) || defined(STYLEOS_TEST_RUNTIME_IMAGE_ADMISSION_865)
#ifndef STYLEOS_TEST_WIDE_ROOTS_864
#error verified image admission requires lease-bound root metadata
#endif
enum ps_status rt865_bank_admit_image(struct rt862_bank *b,struct dpr_service *d,
 uint64_t live,unsigned slot,const struct s846_root *child,const struct s844_lease *lease,
 const struct nte_loaded *image,const char *name,const struct up_context *initial,uint32_t *pid)
{
 if(!layout(b,d,child,sizeof *child,_Alignof(struct s846_root),lease,sizeof *lease,_Alignof(struct s844_lease))||
    !layout(b,d,image,sizeof *image,_Alignof(struct nte_loaded),initial,sizeof *initial,_Alignof(struct up_context))||
    !layout(b,d,pid,sizeof *pid,_Alignof(uint32_t),0,0,1)||
    !apart(child,sizeof *child,image,sizeof *image)||!apart(child,sizeof *child,initial,sizeof *initial)||
    !apart(lease,sizeof *lease,image,sizeof *image)||!apart(lease,sizeof *lease,initial,sizeof *initial)||
    !apart(pid,sizeof *pid,child,sizeof *child)||!apart(pid,sizeof *pid,lease,sizeof *lease)||
    !apart(pid,sizeof *pid,image,sizeof *image)||!apart(pid,sizeof *pid,initial,sizeof *initial)||
    slot>=S846_ROOT_COUNT||slot>=PS_MAX_SLOTS||!child->ready||child->built.error!=PG_OK||
    !root(child->built.root)||child->built.root==live||child->built.root!=child->plan.tables_pa||
    !child->built.used||child->built.used>child->plan.capacity||child->plan.capacity!=PG_TABLE_PAGES||
    child->plan.arch!=PG_X86||child->plan.count<3||child->plan.count>PG_MAX_MAPS||
    !root(lease->pa)||lease->pa>(UINT64_C(1)<<47)-3*PAGE||lease->pages!=3||
    lease->owner!=S846_OWNER_A+slot||!lease->generation||
    child->lease.pa!=lease->pa||child->lease.owner!=lease->owner||
    child->lease.pages!=lease->pages||child->lease.generation!=lease->generation||
    !image->code_bytes||image->code_bytes>image->memory_bytes||image->memory_bytes>PAGE||
    image->entry<S846_CODE||image->entry-S846_CODE>=image->code_bytes||
    initial->pc!=image->entry)return PS_EVIDENCE;
 const uint64_t vas[3]={S846_CODE,S846_DATA,S846_STACK};
 for(unsigned i=0;i<3;i++){
  const struct pg_mapping *m=&child->plan.maps[child->plan.count-3+i];
  if(m->va!=vas[i]||m->pa!=lease->pa+i*PAGE||m->bytes!=PAGE||
     m->flags!=(i?PG_WRITE:PG_EXEC))return PS_EVIDENCE;
 }
 return rt863_bank_admit_initial(b,d,live,slot,lease->generation,child->built.root,name,initial,pid);
}
#endif

/* Supervisor receipt after actual trap, safe-root restoration and FP save.
 * RANGE only validates a user interval; no user dereference. A returning
 * syscall is a scheduling boundary: save the result, transition to READY,
 * and let the existing dispatcher choose the next context. */
enum ps_status rt863_bank_syscall(struct rt862_bank *b,struct dpr_service *d,
 const struct rt862_selection *t,uint64_t live,uint64_t safe_flags,
 const struct s847_raw *f,uint64_t va,uint64_t cr2){
 if(!layout(b,d,t,sizeof *t,_Alignof(struct rt862_selection),f,sizeof *f,_Alignof(struct s847_raw))||
    !ticket(b,d,t,live)||(safe_flags&~UINT64_C(0x8d7))||!(safe_flags&2)||
    (f->rflags&0x202)!=0x202||f->rsp<=S846_STACK||cr2||f->vector!=128||
    f->gpr[0]==S847_CALL_EXIT)return PS_EVIDENCE;
 struct s847_raw normalized=*f;normalized.rflags&=~IF;
 struct up_context saved;struct up_event event;
 if(!s847_decode(&normalized,va,cr2,t->child_root,b->cs,b->ss,&saved,&event)||
    event.kind!=UP_EVENT_NONE||!context(b,&saved))return PS_EVIDENCE;
 uint64_t result=UC772_BAD;
 if(f->gpr[0]==S847_CALL_YIELD)result=0;
 else if(f->gpr[0]==S847_CALL_RANGE){size_t offset=0;
    result=uc772_plan(event.active_root,t->child_root,S846_DATA,f->gpr[1],f->gpr[2],&offset);}
 saved.gpr[0]=result;
 enum ps_status status=dpr_observe(d,t->slot,t->handle,t->child_root,live,PS_YIELD);
 if(status!=PS_OK)return status;
 b->slots[t->slot].context=saved;return PS_OK;
}
#endif

#ifdef STYLEOS_TEST_RUNTIME_COOP_863
#include <styleos/user_copy_772.h>
/* Separate IF=0 integer-only fixture. No fabricated interrupt bit and no
 * relaxation of the existing timer-profile gates. */
bool rt863_coop_enter(const struct rt862_bank *b,const struct dpr_service *d,
 const struct rt862_selection *t,uint64_t live,struct up_context *out){
 if(!layout(b,d,t,sizeof *t,_Alignof(struct rt862_selection),out,sizeof *out,_Alignof(struct up_context))||!ticket(b,d,t,live))return false;
 *out=b->slots[t->slot].context;return true;
}
enum ps_status rt863_coop_commit(struct rt862_bank *b,struct dpr_service *d,
 const struct rt862_selection *t,uint64_t live,uint64_t flags,
 const struct s847_raw *f,uint64_t va,uint64_t cr2){
 if(!layout(b,d,t,sizeof *t,_Alignof(struct rt862_selection),f,sizeof *f,_Alignof(struct s847_raw))||
    !ticket(b,d,t,live)||(flags&~UINT64_C(0x8d7))||!(flags&2)||f->rsp<=S846_STACK)return PS_EVIDENCE;
 struct up_context saved;struct up_event event;
 if(!s847_decode(f,va,cr2,t->child_root,b->cs,b->ss,&saved,&event)||!context(b,&saved))return PS_EVIDENCE;
 enum ps_event kind=PS_YIELD;
 if(event.kind==UP_FAULT)kind=PS_FAULT;
 else if(cr2)return PS_EVIDENCE;
 else if(f->gpr[0]==S847_CALL_EXIT)kind=PS_EXIT;
 else if(f->gpr[0]==S847_CALL_YIELD)saved.gpr[0]=0;
 else if(f->gpr[0]==S847_CALL_RANGE){size_t off=0;saved.gpr[0]=uc772_plan(event.active_root,t->child_root,S846_DATA,f->gpr[1],f->gpr[2],&off);}
 else saved.gpr[0]=UC772_BAD;
 enum ps_status status=dpr_observe(d,t->slot,t->handle,t->child_root,live,kind);
 if(status!=PS_OK)return status;
 if(kind==PS_YIELD)b->slots[t->slot].context=saved;
 else{volatile unsigned char *q=(void *)&b->slots[t->slot].context;for(size_t i=0;i<sizeof saved;i++)q[i]=0;}
 return PS_OK;
}
#endif

#ifdef STYLEOS_TEST_RUNTIME_SYSCALL_863
/* Unified supervisor receipt, after the architecture has authenticated the
 * real trap, suspended FP, restored the safe root/stack and retired the IRQ.
 * A software receipt cannot itself prove any of those hardware facts. */
enum ps_status rt863_bank_event(struct rt862_bank *b,struct dpr_service *d,
 const struct rt862_selection *t,uint64_t live,const struct rt860_tick *r,uint64_t cr2){
 if(!layout(b,d,t,sizeof *t,_Alignof(struct rt862_selection),r,sizeof *r,_Alignof(struct rt860_tick))||
    !ticket(b,d,t,live)||r->slot!=t->slot||r->dispatch!=t->dispatch||r->pid!=t->pid||
    r->handle!=t->handle||r->child_root!=t->child_root||r->safe_root!=live||r->vector!=r->frame.vector)return PS_EVIDENCE;
 if(r->vector==0x50){
  if(cr2||r->isr!=1||r->eoi!=1)return PS_EVIDENCE;
  return rt862_bank_tick(b,d,t,live,r);
 }
 if(r->isr||r->eoi||(r->vector!=128&&r->vector!=14
#ifdef STYLEOS_TEST_FOUR_GUEST_864
     &&r->vector!=6&&r->vector!=13
#endif
     )||(r->vector!=14&&cr2))return PS_EVIDENCE;
 if(r->vector!=128||r->frame.gpr[0]==S847_CALL_EXIT)
  return rt862_bank_finish(b,d,t,live,r->safe_flags,&r->frame,r->frame_va,cr2);
 return rt863_bank_syscall(b,d,t,live,r->safe_flags,&r->frame,r->frame_va,cr2);
}
#endif
