#include <styleos/process_trap_847.h>
#include <styleos/user_copy_772.h>
#define P UINT64_C(4096)
#define LOW (UINT64_C(1)<<47)
#define FLAGS UINT64_C(0x8d7) /* arithmetic flags + reserved bit1, IF=0 */
static bool root_ok(uint64_t r){return r>=P&&r<LOW&&!(r&(P-1));}
static bool region(const void *p,size_t n,size_t a){return p&&!( (uintptr_t)p%a)&&n<=UINTPTR_MAX-(uintptr_t)p;}
static bool separate(const void *a,size_t n,const void *b,size_t m){return (uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a;}
static bool selectors(uint64_t cs,uint64_t ss){return cs<=0xffff&&ss<=0xffff&&(cs&7)==3&&(ss&7)==3&&cs>=11&&ss>=11&&cs!=ss;}
static bool context_ok(const struct s847_control *c,const struct up_context *v){
 if(v->cs!=c->cs||v->ss!=c->ss||!(v->status&2)||(v->status&~FLAGS)||v->pc<S846_CODE||v->pc>=S846_CODE+P||v->sp<S846_STACK||v->sp>S846_STACK+P)return false;
 for(unsigned i=15;i<31;i++)if(v->gpr[i])return false;
 return true;
}
static bool shape(const struct s847_control *c){
 if(!region(c,sizeof *c,_Alignof(struct s847_control))||!c->ready||!root_ok(c->safe)||!selectors(c->cs,c->ss)||c->current< -1||c->current>1||c->next>1||c->slot[0].root==c->slot[1].root)return false;
 for(unsigned i=0;i<2;i++){
  const struct s847_slot *s=&c->slot[i];
  if(!root_ok(s->root)||s->root==c->safe||s->state<S847_READY||s->state>S847_EXITED||!context_ok(c,&s->context)||((s->state==S847_RUNNING)!=(c->current==(int)i)))return false;
 }
 return true;
}
bool s847_init(struct s847_control *c,uint64_t safe,uint64_t a,uint64_t b,uint16_t cs,uint16_t ss){
 if(!region(c,sizeof *c,_Alignof(struct s847_control))||c->ready||!root_ok(safe)||!root_ok(a)||!root_ok(b)||a==b||a==safe||b==safe||!selectors(cs,ss))return false;
 struct s847_control v={.safe=safe,.cs=cs,.ss=ss,.current=-1,.ready=true};
 for(unsigned i=0;i<2;i++){v.slot[i].root=i?b:a;v.slot[i].state=S847_READY;v.slot[i].context=(struct up_context){.sp=S846_STACK+P,.pc=S846_CODE,.status=2,.cs=cs,.ss=ss};}
 *c=v;return true;
}
bool s847_dispatch(struct s847_control *c,uint64_t live_root,unsigned *id,struct up_context *out){
 if(!shape(c)||c->current!=-1||live_root!=c->safe||!region(id,sizeof *id,_Alignof(unsigned))||!region(out,sizeof *out,_Alignof(struct up_context))||!separate(c,sizeof *c,id,sizeof *id)||!separate(c,sizeof *c,out,sizeof *out)||!separate(id,sizeof *id,out,sizeof *out))return false;
 for(unsigned i=0;i<2;i++){unsigned j=(c->next+i)%2;if(c->slot[j].state!=S847_READY)continue;
  *out=c->slot[j].context;*id=j;c->slot[j].state=S847_RUNNING;c->current=(int)j;c->next=1-j;return true;
 }
 return false;
}
bool s847_trap(struct s847_control *c,const struct up_context *f,const struct up_event *e,struct s847_decision *out){
 if(!shape(c)||c->current<0||!region(f,sizeof *f,_Alignof(struct up_context))||!region(e,sizeof *e,_Alignof(struct up_event))||!region(out,sizeof *out,_Alignof(struct s847_decision))||!separate(c,sizeof *c,f,sizeof *f)||!separate(c,sizeof *c,e,sizeof *e)||!separate(c,sizeof *c,out,sizeof *out)||!separate(out,sizeof *out,f,sizeof *f)||!separate(out,sizeof *out,e,sizeof *e)||!context_ok(c,f))return false;
 struct s847_slot *s=&c->slot[c->current];if(e->active_root!=s->root)return false;
 /* The saved raw frame must lie fully inside the matching guarded page.
  * Exact assembly SP / frame provenance is a separate adapter obligation. */
 uint64_t ist=e->vector==128?S846_IST:S846_IST+3*P;
 if(e->kernel_sp<ist||e->kernel_sp>ist+P-176||(e->kernel_sp&7))return false;
 struct s847_decision d={0};struct up_context resume=*f;enum s847_state state=S847_RUNNING;
 if(e->vector==128){
  if(e->kind!=UP_EVENT_NONE||e->syndrome||e->fault_va)return false;
  switch(f->gpr[0]){
   case S847_CALL_YIELD:d.action=S847_YIELD;state=S847_READY;break;
   case S847_CALL_EXIT:d.action=S847_EXIT;state=S847_EXITED;break;
   case S847_CALL_RANGE:{size_t offset=0;d.action=S847_RETURN;d.result=uc772_plan(e->active_root,s->root,S846_DATA,f->gpr[1],f->gpr[2],&offset);break;}
   default:d.action=S847_RETURN;d.result=UC772_BAD;break;
  }
  resume.gpr[0]=d.result;
 }else if(e->vector==14){
  /* Only ordinary user-origin page faults from admitted code. No RSVD,
   * PK, shadow stack or instruction-fetch fault claims in this profile. */
  if(e->kind!=UP_FAULT||!(e->syndrome&4)||(e->syndrome&~UINT64_C(7))||e->fault_va>=LOW)return false;
  d.action=S847_FAULT;state=S847_FAULTED;
 }else return false;
 s->context=resume;s->state=state;if(state!=S847_RUNNING)c->current=-1;*out=d;return true;
}

/* S848: distinct hardware tick transition; do not synthesize an INT80 yield.
 * Stored context uses the existing IF=0 convention. The IRQ bridge validates
 * the actual saved IF and restores it only on entry to the user program. */
bool s848_suspend_tick(struct s847_control *c,const struct up_context *f,uint64_t live_root){
 if(!shape(c)||c->current<0||!region(f,sizeof *f,_Alignof(struct up_context))||
 !separate(c,sizeof *c,f,sizeof *f)||!context_ok(c,f))return false;
 struct s847_slot *s=&c->slot[c->current];
 if(s->root!=live_root)return false;
 s->context=*f;s->state=S847_READY;c->current=-1;return true;
}
