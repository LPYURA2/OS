#include <styleos/runtime_bank_862.h>
#include <styleos/user_copy_772.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank b;
static struct dpr_service d;
static struct pr_table p;
static struct ws_service w;
static unsigned pos,neg;
#define CHECK(x) do{assert(x);pos++;}while(0)
#define REJECT(x) do{struct rt862_bank b0=b;struct dpr_service d0=d;struct pr_table p0=p;struct ws_service w0=w;assert(!(x));assert(!memcmp(&b0,&b,sizeof b)&&!memcmp(&d0,&d,sizeof d)&&!memcmp(&p0,&p,sizeof p)&&!memcmp(&w0,&w,sizeof w));neg++;}while(0)
static struct rt862_selection select_slot(unsigned want){unsigned i;uint32_t pid;CHECK(dpr_dispatch(&d,&i,&pid)==PS_OK&&i==want);return (struct rt862_selection){i,d.dispatcher.dispatches,pid,d.dispatcher.slots[i].handle,d.dispatcher.slots[i].user_root,d.dispatcher.safe_root};}
static struct rt860_tick receipt(const struct rt862_selection *t,const struct up_context *c,unsigned vector){
 struct rt860_tick r={.slot=t->slot,.dispatch=t->dispatch,.pid=t->pid,.handle=t->handle,.child_root=t->child_root,.safe_root=t->safe_root,.frame_va=S846_IST+4096-16-sizeof(struct s847_raw),.vector=vector,.isr=vector==0x50,.eoi=vector==0x50,.safe_flags=2,.frame={.rip=c->pc,.rsp=c->sp,.cs=c->cs,.ss=c->ss,.rflags=c->status,.vector=vector}};
 for(unsigned i=0;i<15;i++)r.frame.gpr[i]=c->gpr[i];
 return r;
}
int main(void){
 uint32_t pid[2],win[2];unsigned ticks[2]={0};pr_init(&p);ws_init(&w);CHECK(dpr_start(&d,&p,&w,0x1000)==DPR_OK);
 for(unsigned i=0;i<2;i++){CHECK(dpr_admit(&d,i,101+i,0x2000+4096*i,0,i?"B":"A",&pid[i])==PS_OK);CHECK(ws_open_app(&w,&p,pid[i],i?"B":"A",(struct ws_bounds){0,0,400,300},&win[i])==WS_OK);}
 CHECK(ws_set_minimized(&w,pid[0],win[0],true)==WS_OK);CHECK(rt862_bank_init(&b,&d,0x1000,0x5b,0x63));
 struct rt862_selection old={0};struct rt860_tick prior={0};
 for(unsigned i=0;i<256;i++){
  unsigned id=i%2,kind=(i/2)%4;bool irq=kind==0||kind==2;
  struct rt862_selection t=select_slot(id);struct up_context c={0},other=b.slots[1-id].context;
  CHECK(rt862_bank_enter(&b,&d,&t,0x1000,&c)&&c.status==0x202);
  struct rt860_tick r=receipt(&t,&c,irq?0x50:128);r.frame.rip=S846_CODE+1+i;
  r.frame.gpr[14]=i+42;
  if(!irq){r.frame.gpr[0]=S847_CALL_RANGE;r.frame.gpr[1]=kind==1?S846_DATA:UINT64_MAX;r.frame.gpr[2]=8;}
  struct rt860_tick bad=r;bad.vector^=1;REJECT(rt863_bank_event(&b,&d,&t,0x1000,&bad,0)==PS_OK);
  bad=r;bad.eoi^=1;REJECT(rt863_bank_event(&b,&d,&t,0x1000,&bad,0)==PS_OK);
  bad=r;bad.isr^=1;REJECT(rt863_bank_event(&b,&d,&t,0x1000,&bad,0)==PS_OK);
  bad=r;bad.dispatch++;REJECT(rt863_bank_event(&b,&d,&t,0x1000,&bad,0)==PS_OK);
  bad=r;bad.handle++;REJECT(rt863_bank_event(&b,&d,&t,0x1000,&bad,0)==PS_OK);
  bad=r;bad.child_root^=4096;REJECT(rt863_bank_event(&b,&d,&t,0x1000,&bad,0)==PS_OK);
  REJECT(rt863_bank_event(&b,&d,&old,0x1000,&prior,0)==PS_OK);
  REJECT(rt863_bank_event(&b,&d,&t,0x1000,&r,1)==PS_OK);
  CHECK(rt863_bank_event(&b,&d,&t,0x1000,&r,0)==PS_OK);ticks[id]+=irq;
  CHECK(pr_find(&p,pid[id])->cpu_ticks==ticks[id]&&pr_find(&p,pid[id])->state==PR_READY);
  CHECK(b.slots[id].context.pc==r.frame.rip&&b.slots[id].context.status==2&&b.slots[id].context.gpr[14]==i+42&&b.slots[id].context.gpr[0]==(irq?r.frame.gpr[0]:kind==1?UC772_OK:UC772_RANGE));
  CHECK(!memcmp(&other,&b.slots[1-id].context,sizeof other)&&ws_find(&w,win[0])->minimized&&ws_find(&w,win[1]));
  REJECT(rt863_bank_event(&b,&d,&t,0x1000,&r,0)==PS_OK);old=t;prior=r;
 }
 for(unsigned id=0;id<2;id++){
  struct rt862_selection t=select_slot(id);struct up_context c;CHECK(rt862_bank_enter(&b,&d,&t,0x1000,&c));struct rt860_tick r=receipt(&t,&c,id?128:14);
  if(id)r.frame.gpr[0]=S847_CALL_EXIT;else{r.frame.error=5;r.frame_va+=3*4096;}
  struct rt860_tick bad=r;bad.eoi=1;REJECT(rt863_bank_event(&b,&d,&t,0x1000,&bad,id?0:S846_IST)==PS_OK);
  CHECK(rt863_bank_event(&b,&d,&t,0x1000,&r,id?0:S846_IST)==PS_OK);
  CHECK(pr_find(&p,pid[id])->state==(id?PR_EXITED:PR_FAULTED)&&!ws_find(&w,win[id])&&pr_find(&p,pid[id])->cpu_ticks==64);
 }
 CHECK(rt862_bank_release(&b,&d,0x1000,0,pid[0],101,0x2000)==PS_OK);CHECK(rt862_bank_release(&b,&d,0x1000,1,pid[1],102,0x3000)==PS_OK);
 CHECK(!pr_snapshot(&p,0,0,true)&&!ws_snapshot(&w,0,0)&&d.dispatcher.dispatches==258);
 printf("S863 mixed event HOST ONLY: %u positive / %u refusals PASS; 128 ticks, 128 returning calls, 2 terminal events\n",pos,neg);
}
