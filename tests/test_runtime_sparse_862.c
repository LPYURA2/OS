#include <styleos/runtime_bank_862.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank b;
static struct dpr_service d;
static struct pr_table p;
static struct ws_service w;
static unsigned good,bad;
#define OK(x) do{assert(x);good++;}while(0)
#define NO(x) do{struct rt862_bank b0=b;struct dpr_service d0=d;struct pr_table p0=p;struct ws_service w0=w;assert(!(x));assert(!memcmp(&b,&b0,sizeof b)&&!memcmp(&d,&d0,sizeof d)&&!memcmp(&p,&p0,sizeof p)&&!memcmp(&w,&w0,sizeof w));bad++;}while(0)
static struct rt862_selection choose(unsigned expected,uint32_t pid){unsigned i;uint32_t x;OK(dpr_dispatch(&d,&i,&x)==PS_OK&&i==expected&&x==pid);return (struct rt862_selection){i,d.dispatcher.dispatches,x,d.dispatcher.slots[i].handle,d.dispatcher.slots[i].user_root,d.dispatcher.safe_root};}
static void tick(const struct rt862_selection *t){struct up_context c;OK(rt862_bank_enter(&b,&d,t,0x1000,&c));struct rt860_tick r={.slot=t->slot,.dispatch=t->dispatch,.pid=t->pid,.handle=t->handle,.child_root=t->child_root,.safe_root=t->safe_root,.frame_va=S846_IST+4096-16-sizeof(struct s847_raw),.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,.frame={.vector=0x50,.rip=c.pc,.rsp=c.sp,.cs=c.cs,.ss=c.ss,.rflags=c.status,.gpr={[0]=0x862}}};OK(rt862_bank_tick(&b,&d,t,0x1000,&r)==PS_OK);OK(pr_find(&p,t->pid)->cpu_ticks==1);NO(rt862_bank_tick(&b,&d,t,0x1000,&r)==PS_OK);}
static void finish(const struct rt862_selection *t){struct up_context c;OK(rt862_bank_enter(&b,&d,t,0x1000,&c));struct s847_raw f={.vector=128,.rip=c.pc,.rsp=c.sp,.cs=c.cs,.ss=c.ss,.rflags=c.status,.gpr={[0]=S847_CALL_EXIT}};OK(rt862_bank_finish(&b,&d,t,0x1000,2,&f,S846_IST+4096-16-sizeof f,0)==PS_OK);}
int main(void){pr_init(&p);ws_init(&w);OK(dpr_start(&d,&p,&w,0x1000)==DPR_OK);OK(rt862_bank_open(&b,&d,0x1000,0x5b,0x63));NO(rt862_bank_open(&b,&d,0x1000,0x5b,0x63));unsigned id;uint32_t pid,prev=0;struct rt862_selection stale={0};
 for(unsigned n=0;n<128;n++){
  unsigned i=n%2;uint64_t root=0x2000+4096*i,handle=101+i;pid=0;
  NO(rt862_bank_admit(&b,&d,0x2000,i,handle,root,"A",&pid)==PS_OK);OK(!pid);
  NO(rt862_bank_admit(&b,&d,0x1000,i,handle,0x1000,"A",&pid)==PS_OK);OK(!pid);
  NO(rt862_bank_admit(&b,&d,0x1000,i,handle,root,"",&pid)==PS_OK);OK(!pid);
  OK(rt862_bank_admit(&b,&d,0x1000,i,handle,root,"A",&pid)==PS_OK&&pid>prev);
  NO(rt862_bank_admit(&b,&d,0x1000,1-i,handle,root+0x2000,"B",&pid)==PS_OK);
  NO(rt862_bank_release(&b,&d,0x1000,i,pid,handle,root)==PS_OK);
  uint32_t win;OK(ws_open_app(&w,&p,pid,"A",(struct ws_bounds){0,0,400,300},&win)==WS_OK);
  struct rt862_selection t=choose(i,pid);struct up_context c={0};
  if(n)NO(rt862_bank_enter(&b,&d,&stale,0x1000,&c));
  NO(rt862_bank_admit(&b,&d,0x1000,1-i,105,0x5000,"B",&pid)==PS_OK);
  tick(&t);t=choose(i,pid);OK(rt862_bank_enter(&b,&d,&t,0x1000,&c)&&c.gpr[0]==0x862);finish(&t);OK(!ws_find(&w,win));
  NO(rt862_bank_release(&b,&d,0x1000,i,pid+1,handle,root)==PS_OK);
  NO(rt862_bank_release(&b,&d,0x1000,i,pid,handle+1,root)==PS_OK);
  OK(rt862_bank_release(&b,&d,0x1000,i,pid,handle,root)==PS_OK);
  NO(rt862_bank_release(&b,&d,0x1000,i,pid,handle,root)==PS_OK);
  OK(!pr_snapshot(&p,0,0,true)&&dpr_dispatch(&d,&id,&pid)==PS_EMPTY);prev=t.pid;stale=t;
 }
 /* A ready survivor while a different slot is reaped and reused. */
 uint32_t a,z,next;OK(rt862_bank_admit(&b,&d,0x1000,0,101,0x2000,"A",&a)==PS_OK);OK(rt862_bank_admit(&b,&d,0x1000,1,102,0x3000,"B",&z)==PS_OK);
 struct rt862_selection t=choose(0,a);finish(&t);OK(rt862_bank_release(&b,&d,0x1000,0,a,101,0x2000)==PS_OK);OK(rt862_bank_admit(&b,&d,0x1000,0,101,0x2000,"C",&next)==PS_OK&&next>z);
 t=choose(1,z);finish(&t);OK(rt862_bank_release(&b,&d,0x1000,1,z,102,0x3000)==PS_OK);t=choose(0,next);finish(&t);OK(rt862_bank_release(&b,&d,0x1000,0,next,101,0x2000)==PS_OK);OK(!pr_snapshot(&p,0,0,true)&&!ws_snapshot(&w,0,0)&&dpr_dispatch(&d,&id,&pid)==PS_EMPTY);
 printf("S862 sparse HOST ONLY: %u positive / %u refusals; 131 admissions and releases, 128 synthetic ticks PASS\n",good,bad);return 0;}
