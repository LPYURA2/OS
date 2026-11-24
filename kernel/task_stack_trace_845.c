#include <styleos/task_stack_trace_845.h>
#define LOW (UINT64_C(1)<<47)
static bool valid(const struct s844_lease *l,const struct s843_task *t,unsigned i){
 return l->pa>=4096&&!(l->pa&4095)&&l->pa<=LOW-TASKS_STACK_BYTES&&l->generation&&l->pages==4&&
 l->owner==(i?S845_OWNER_B:S845_OWNER_A)&&t->stack.start==l->pa&&t->stack.end==l->pa+TASKS_STACK_BYTES;
}
bool s845_observe(struct s845_trace *trace,const struct s845_pair *pair,const struct s843_scheduler *s,const struct s843_frame *f,uint64_t a,uint64_t b){
 if(!trace||!pair||!s||!f||!pair->live||!s->ready||s->control.current<0||s->control.current>=2||s->ticks!=trace->ticks||trace->ticks>=16)return false;
 unsigned cur=(unsigned)s->control.current,other=1-cur;
 if(cur!=trace->ticks%2||!valid(&pair->lease[0],&s->task[0],0)||!valid(&pair->lease[1],&s->task[1],1)||pair->lease[0].pa+TASKS_STACK_BYTES>pair->lease[1].pa||pair->lease[0].generation==pair->lease[1].generation)return false;
 if(f->rsp!=s->task[cur].stack.end-16||f->rip<s->task[cur].code.start||f->rip>=s->task[cur].code.end)return false;
 uint64_t counts[2],progress[2]={a,b};
 for(unsigned i=0;i<2;i++){
  const volatile uint64_t *v=(const volatile uint64_t *)(uintptr_t)(s->task[i].stack.end-24);
  counts[i]=*v;
  if(counts[i]<progress[i]||counts[i]-progress[i]>1)return false;
 }
 if(progress[cur]<=trace->progress[cur]||counts[cur]<=trace->count[cur]||progress[other]!=trace->progress[other]||counts[other]!=trace->count[other])return false;
 trace->count[0]=counts[0];trace->count[1]=counts[1];trace->progress[0]=a;trace->progress[1]=b;trace->ticks++;return true;
}
