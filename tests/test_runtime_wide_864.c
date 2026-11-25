#include <styleos/runtime_bank_862.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
_Static_assert(PS_MAX_SLOTS==32,"wide scheduler test only");
static struct rt862_bank bank;
static struct dpr_service service;
static struct pr_table registry;
static struct ws_service windows;
static uint32_t pids[PS_MAX_SLOTS],wins[PS_MAX_SLOTS];
static uint64_t handles[PS_MAX_SLOTS];
static unsigned passes,refusals,dispatches,admissions,ticks;
#define OK(x) do{assert(x);++passes;}while(0)
#define NO(x) do{struct rt862_bank b=bank;struct dpr_service d=service;struct pr_table p=registry;struct ws_service w=windows;assert(!(x));assert(!memcmp(&b,&bank,sizeof b)&&!memcmp(&d,&service,sizeof d)&&!memcmp(&p,&registry,sizeof p)&&!memcmp(&w,&windows,sizeof w));++refusals;}while(0)
static uint64_t root(unsigned i){return 0x2000+(uint64_t)i*4096;}
static void admit(unsigned i,uint64_t handle)
{
 struct up_context c={.pc=S846_CODE+i,.sp=S846_STACK+4096-16*i,.status=2,.cs=0x5b,.ss=0x63};
 c.gpr[14]=handle;
 uint32_t pid=0;
 OK(rt863_bank_admit_initial(&bank,&service,0x1000,i,handle,root(i),"wide",&c,&pid)==PS_OK);
 OK(pid>pids[i]);pids[i]=pid;handles[i]=handle;++admissions;
 if(i<4)OK(ws_open_app(&windows,&registry,pid,"wide",(struct ws_bounds){0,0,400,300},&wins[i])==WS_OK);
}
static struct rt862_selection choose(unsigned i)
{
 unsigned s;uint32_t pid;
 OK(dpr_dispatch(&service,&s,&pid)==PS_OK&&s==i&&pid==pids[i]);++dispatches;
 return (struct rt862_selection){s,service.dispatcher.dispatches,pid,handles[i],root(i),0x1000};
}
static struct rt860_tick receipt(const struct rt862_selection *t,const struct up_context *c,unsigned vector)
{
 struct rt860_tick r={.slot=t->slot,.dispatch=t->dispatch,.pid=t->pid,.handle=t->handle,.child_root=t->child_root,.safe_root=t->safe_root,
 .frame_va=S846_IST+4096-16-sizeof(struct s847_raw),.vector=vector,.isr=vector==0x50,.eoi=vector==0x50,.safe_flags=2,
 .frame={.vector=vector,.rip=c->pc,.rsp=c->sp,.cs=c->cs,.ss=c->ss,.rflags=c->status}};
 for(unsigned j=0;j<15;j++)r.frame.gpr[j]=c->gpr[j];
 return r;
}
static void retire(unsigned i,const struct rt862_selection *t,const struct up_context *c,bool fault)
{
 struct rt860_tick r=receipt(t,c,fault?14:128);
 if(fault){r.frame.error=5;r.frame_va+=3*4096;}else r.frame.gpr[0]=S847_CALL_EXIT;
 OK(rt863_bank_event(&bank,&service,t,0x1000,&r,fault?S846_IST:0)==PS_OK);
 OK(pr_find(&registry,pids[i])->state==(fault?PR_FAULTED:PR_EXITED));
 if(i<4)OK(!ws_find(&windows,wins[i]));
 NO(rt862_bank_release(&bank,&service,0x1000,i,pids[i],handles[i]+1,root(i))==PS_OK);
 OK(rt862_bank_release(&bank,&service,0x1000,i,pids[i],handles[i],root(i))==PS_OK);
}
int main(void)
{
 pr_init(&registry);ws_init(&windows);
 OK(dpr_start(&service,&registry,&windows,0x1000)==DPR_OK);
 OK(rt862_bank_open(&bank,&service,0x1000,0x5b,0x63));
 for(unsigned i=0;i<PS_MAX_SLOTS;i++)admit(i,100+i);
 OK(pr_snapshot(&registry,0,0,true)==32);
 uint32_t unchanged=0xdeadbeef;
 NO(rt862_bank_admit(&bank,&service,0x1000,32,999,0x90000,"overflow",&unchanged)==PS_OK);
 OK(unchanged==0xdeadbeef);
 struct rt862_selection old={0};struct rt860_tick prior={0};
 for(unsigned round=0;round<64;round++)for(unsigned i=0;i<PS_MAX_SLOTS;i++){
  struct rt862_selection t=choose(i);struct up_context c={0};
  OK(rt862_bank_enter(&bank,&service,&t,0x1000,&c)&&c.gpr[14]==handles[i]);
  if(dispatches>1)NO(rt863_bank_event(&bank,&service,&old,0x1000,&prior,0)==PS_OK);
  struct rt860_tick r=receipt(&t,&c,round%2?128:0x50);
  if(round%2)r.frame.gpr[0]=S847_CALL_YIELD;
  struct rt860_tick wrong=r;wrong.pid=pids[(i+1)%32];
  NO(rt863_bank_event(&bank,&service,&t,0x1000,&wrong,0)==PS_OK);
  OK(rt863_bank_event(&bank,&service,&t,0x1000,&r,0)==PS_OK);
  if(!(round%2))++ticks;
  OK(pr_find(&registry,pids[i])->cpu_ticks==(round/2+1));
  OK(pr_find(&registry,pids[i])->state==PR_READY);
  NO(rt863_bank_event(&bank,&service,&t,0x1000,&r,0)==PS_OK);
  old=t;prior=r;
 }
 /* Fault every even slot, refill it, and preserve each surviving context. */
 for(unsigned i=0;i<32;i++){
  struct rt862_selection t=choose(i);struct up_context c={0};
  OK(rt862_bank_enter(&bank,&service,&t,0x1000,&c));
  if(!(i%2)){
   retire(i,&t,&c,true);admit(i,1000+i);
   NO(rt862_bank_enter(&bank,&service,&t,0x1000,&c));
  }else{
   struct rt860_tick r=receipt(&t,&c,128);r.frame.gpr[0]=S847_CALL_YIELD;
   OK(rt863_bank_event(&bank,&service,&t,0x1000,&r,0)==PS_OK);
  }
 }
 OK(pr_snapshot(&registry,0,0,true)==32);
 for(unsigned i=0;i<32;i++){
  struct rt862_selection t=choose(i);struct up_context c={0};
  OK(rt862_bank_enter(&bank,&service,&t,0x1000,&c)&&c.gpr[14]==handles[i]);
  OK(pr_find(&registry,pids[i])->cpu_ticks==(i%2?32u:0u));
  retire(i,&t,&c,false);
 }
 unsigned slot;uint32_t pid;
 OK(dpr_dispatch(&service,&slot,&pid)==PS_EMPTY);
 OK(!pr_snapshot(&registry,0,0,true)&&!ws_snapshot(&windows,0,0));
 OK(dispatches==2112&&admissions==48&&ticks==1024);
 printf("S864 wide HOST ONLY: %u admissions, %u dispatches, %u synthetic ticks, %u checks, %u refusals PASS\n",admissions,dispatches,ticks,passes,refusals);
}
