#include <styleos/timer_context_843.h>
#include <limits.h>
#define PAGE UINT64_C(4096)
#define LOW (UINT64_C(1)<<47)
/* The terminal test permits only ordinary arithmetic flags plus IF and bit 1.
 * No debug, direction, nested-task, IOPL, VM, or alignment-check state. */
#define FLAGS_ALLOWED UINT64_C(0xad7)
static bool valid_range(struct boot_range r){return r.start&&r.start<r.end&&r.end<=LOW;}
static bool overlap(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static bool contains(struct boot_range r,uint64_t p,uint64_t n){return p>=r.start&&p<r.end&&n<=r.end-p;}
static struct boot_range object(const void *p,size_t n){uint64_t a=(uintptr_t)p;return (struct boot_range){a,a+n};}
static bool shape(const struct s843_scheduler *s){
 if(!s||!s->ready||!s->root||(s->root&(PAGE-1))||s->root>=LOW||!s->cs||s->cs>0xffff||(s->cs&7)||s->ss>0xffff||(s->ss&7)||!valid_range(s->ist)||s->ist.end-s->ist.start<PAGE||((s->ist.start|s->ist.end)&(PAGE-1))||overlap(s->ist,object(s,sizeof *s)))return false;
 if(s->control.current<0||s->control.current>=(int)TASKS_MAX||s->control.next!=(unsigned)(1-s->control.current)||s->control.completed||s->ticks>=UINT_MAX||s->control.dispatches!=s->ticks+1)return false;
 uint64_t yields=0;
 for(unsigned i=0;i<TASKS_MAX;i++){
  const struct s843_task *t=&s->task[i];const struct tasks_slot *k=&s->control.slots[i];
  if(!valid_range(t->code)||!valid_range(t->stack)||!contains(t->code,t->entry,1)||t->stack.end-t->stack.start!=TASKS_STACK_BYTES||((t->stack.start|t->stack.end)&(PAGE-1))||overlap(t->stack,s->ist)||overlap(t->code,s->ist)||overlap(t->stack,object(s,sizeof *s))||overlap(t->code,object(s,sizeof *s))||k->entry!=(tasks_entry)(uintptr_t)t->entry||k->stack.start!=t->stack.start||k->stack.end!=t->stack.end||k->state!=(i==(unsigned)s->control.current?TASKS_RUNNING:TASKS_READY))return false;
  for(unsigned j=0;j<TASKS_MAX;j++)if(overlap(t->stack,s->task[j].code)||(j!=i&&overlap(t->stack,s->task[j].stack)))return false;
  yields+=k->yields;
 }
 return yields==s->ticks;
}
static bool frame_ok(const struct s843_scheduler *s,unsigned i,const struct s843_frame *f){
 return f->cs==s->cs&&f->ss==s->ss&&(f->rflags&UINT64_C(0x202))==0x202&&!(f->rflags&~FLAGS_ALLOWED)&&contains(s->task[i].code,f->rip,1)&&contains(s->task[i].stack,f->rsp,1)&&f->rsp>=s->task[i].stack.start+256&&!(f->rsp&7);
}
bool s843_init(struct s843_scheduler *out,const struct s843_task t[TASKS_MAX],struct boot_range ist,uint64_t root,uint64_t cs,uint64_t ss){
 if(!out||!t||overlap(object(out,sizeof *out),object(t,TASKS_MAX*sizeof *t)))return false;
 struct s843_scheduler s={0};s.ist=ist;s.root=root;s.cs=cs;s.ss=ss;tasks_init(&s.control);
 for(unsigned i=0;i<TASKS_MAX;i++){
  s.task[i]=t[i];unsigned id=99;
  if(tasks_create(&s.control,(tasks_entry)(uintptr_t)t[i].entry,t[i].stack,&id)!=TASKS_OK||id!=i)return false;
  s.saved[i]=(struct s843_frame){.rip=t[i].entry,.cs=cs,.rflags=0x202,.rsp=t[i].stack.end-16,.ss=ss};
 }
 unsigned id=99;if(tasks_go(&s.control,&id)!=TASKS_OK||id!=0)return false;s.ready=true;
 /* The candidate is local, so also verify the final destination against all
  * stack/code ranges before committing it. */
 if(!shape(&s)||overlap(object(out,sizeof *out),ist))return false;
 for(unsigned i=0;i<TASKS_MAX;i++)if(overlap(object(out,sizeof *out),t[i].stack)||overlap(object(out,sizeof *out),t[i].code)||!frame_ok(&s,i,&s.saved[i]))return false;
 *out=s;return true;
}
bool s843_first(const struct s843_scheduler *s,struct s843_frame *out){
 if(!shape(s)||!out||s->ticks||s->control.current!=0||!frame_ok(s,0,&s->saved[0])||((uintptr_t)out&7)||!contains(s->ist,(uintptr_t)out,sizeof *out))return false;
 *out=s->saved[0];return true;
}
bool s843_tick(struct s843_scheduler *s,struct s843_frame *f,uint64_t live_root){
 if(!shape(s)||!f||live_root!=s->root||((uintptr_t)f&7)||!contains(s->ist,(uintptr_t)f,sizeof *f)||s->ticks>=UINT_MAX-1)return false;
 unsigned cur=(unsigned)s->control.current,next=1-cur;
 if(!frame_ok(s,cur,f)||!frame_ok(s,next,&s->saved[next]))return false;
 struct tasks_control candidate=s->control;unsigned chosen=99;
 if(tasks_mark_yield(&candidate)!=TASKS_OK||tasks_go(&candidate,&chosen)!=TASKS_OK||chosen!=next)return false;
 struct s843_frame resume=s->saved[next];
 s->saved[cur]=*f;s->control=candidate;s->ticks++;*f=resume;
 return true;
}

enum s843_irq_action s843_irq_progress(const struct s843_scheduler *s,const struct s843_frame *f,uint64_t root,const uint64_t prev[2],const uint64_t now[2],unsigned defers){
 if(!shape(s)||!f||!prev||!now||root!=s->root||((uintptr_t)f&7)||!contains(s->ist,(uintptr_t)f,sizeof *f)||s->ticks>=UINT_MAX-1||defers>S843_MAX_DEFER)return S843_IRQ_BAD;
 unsigned cur=(unsigned)s->control.current,next=1-cur;
 if(!frame_ok(s,cur,f)||!frame_ok(s,next,&s->saved[next])||f->rsp!=s->task[cur].stack.end-16||now[next]!=prev[next]||now[cur]<prev[cur])return S843_IRQ_BAD;
 if(now[cur]>prev[cur])return S843_IRQ_SWITCH;
 return defers<S843_MAX_DEFER?S843_IRQ_DEFER:S843_IRQ_BAD;
}
