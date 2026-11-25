#include <styleos/runtime_bank_862.h>
#include <styleos/user_copy_772.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank bank;
static struct dpr_service d;
static struct pr_table p;
static struct ws_service w;
static unsigned pos,neg;
#define CHECK(x) do{assert(x);pos++;}while(0)
#define REJECT(x) do{struct rt862_bank b0=bank;struct dpr_service d0=d;struct pr_table p0=p;struct ws_service w0=w;assert(!(x));assert(!memcmp(&b0,&bank,sizeof bank)&&!memcmp(&d0,&d,sizeof d)&&!memcmp(&p0,&p,sizeof p)&&!memcmp(&w0,&w,sizeof w));neg++;}while(0)
static struct rt862_selection choose(unsigned want){unsigned i;uint32_t pid;CHECK(dpr_dispatch(&d,&i,&pid)==PS_OK&&i==want);return (struct rt862_selection){i,d.dispatcher.dispatches,pid,d.dispatcher.slots[i].handle,d.dispatcher.slots[i].user_root,d.dispatcher.safe_root};}
static struct s847_raw raw(const struct up_context *c){struct s847_raw f={.rip=c->pc,.rsp=c->sp,.cs=c->cs,.ss=c->ss,.rflags=c->status,.vector=128};for(unsigned i=0;i<15;i++)f.gpr[i]=c->gpr[i];return f;}
int main(void){
 uint32_t pid[2],win[2];pr_init(&p);ws_init(&w);CHECK(dpr_start(&d,&p,&w,0x1000)==DPR_OK);
 for(unsigned i=0;i<2;i++){CHECK(dpr_admit(&d,i,101+i,0x2000+4096*i,0,i?"B":"A",&pid[i])==PS_OK);CHECK(ws_open_app(&w,&p,pid[i],i?"B":"A",(struct ws_bounds){0,0,400,300},&win[i])==WS_OK);}
 CHECK(ws_set_minimized(&w,pid[0],win[0],true)==WS_OK);CHECK(rt862_bank_init(&bank,&d,0x1000,0x5b,0x63));
 struct up_context expected[2]={bank.slots[0].context,bank.slots[1].context},zero={0};
 const uint64_t va=S846_IST+4096-16-sizeof(struct s847_raw);struct rt862_selection old={0};
 for(unsigned i=0;i<128;i++){
  unsigned id=i%2;struct rt862_selection t=choose(id);struct up_context c={0};
  CHECK(rt863_coop_enter(&bank,&d,&t,0x1000,&c)&&!memcmp(&c,&expected[id],sizeof c));
  REJECT(rt863_coop_enter(&bank,&d,&old,0x1000,&c));REJECT(rt863_coop_enter(&bank,&d,&t,0x2000,&c));
  struct s847_raw f=raw(&c);f.rip=S846_CODE+1+i;f.gpr[0]=i%8<6?S847_CALL_RANGE:i%8==6?S847_CALL_YIELD:99;
  f.gpr[1]=i%8<2?S846_DATA:i%8<4?S846_DATA+4095:UINT64_MAX;f.gpr[2]=i%8==2?1:8;
  for(unsigned k=3;k<15;k++)f.gpr[k]=((uint64_t)i<<32)|k;
  struct s847_raw bad=f;bad.rflags|=0x200;REJECT(rt863_coop_commit(&bank,&d,&t,0x1000,2,&bad,va,0)==PS_OK);
  bad=f;bad.cs^=1;REJECT(rt863_coop_commit(&bank,&d,&t,0x1000,2,&bad,va,0)==PS_OK);
  bad=f;bad.vector=0x50;REJECT(rt863_coop_commit(&bank,&d,&t,0x1000,2,&bad,va,0)==PS_OK);
  REJECT(rt863_coop_commit(&bank,&d,&t,0x1000,2,&f,va-8,0)==PS_OK);
  REJECT(rt863_coop_commit(&bank,&d,&t,0x1000,0x202,&f,va,0)==PS_OK);
  REJECT(rt863_coop_commit(&bank,&d,&t,0x1000,2,&f,va,1)==PS_OK);
  REJECT(rt863_coop_commit(&bank,&d,&old,0x1000,2,&f,va,0)==PS_OK);
  CHECK(rt863_coop_commit(&bank,&d,&t,0x1000,2,&f,va,0)==PS_OK);
  uint64_t result=i%8<3||i%8==6?0:i%8==7?UC772_BAD:UC772_RANGE;
  expected[id]=c;expected[id].pc=f.rip;for(unsigned k=0;k<15;k++)expected[id].gpr[k]=f.gpr[k];expected[id].gpr[0]=result;
  CHECK(!memcmp(&bank.slots[id].context,&expected[id],sizeof c));CHECK(!memcmp(&bank.slots[1-id].context,&expected[1-id],sizeof c));
  CHECK(pr_find(&p,t.pid)->state==PR_READY&&pr_find(&p,t.pid)->cpu_ticks==0&&ws_find(&w,win[0])->minimized&&ws_find(&w,win[1]));
  REJECT(rt863_coop_commit(&bank,&d,&t,0x1000,2,&f,va,0)==PS_OK);old=t;
 }
 struct rt862_selection t=choose(0);struct up_context c;CHECK(rt863_coop_enter(&bank,&d,&t,0x1000,&c));struct s847_raw f=raw(&c);f.vector=14;f.error=5;
 const uint64_t pf=va+3*4096;REJECT(rt863_coop_commit(&bank,&d,&t,0x1000,2,&f,va,S846_IST)==PS_OK);
 CHECK(rt863_coop_commit(&bank,&d,&t,0x1000,2,&f,pf,S846_IST)==PS_OK);CHECK(!memcmp(&bank.slots[0].context,&zero,sizeof zero)&&pr_find(&p,pid[0])->state==PR_FAULTED&&!ws_find(&w,win[0]));
 CHECK(!memcmp(&bank.slots[1].context,&expected[1],sizeof c)&&ws_find(&w,win[1]));
 t=choose(1);CHECK(rt863_coop_enter(&bank,&d,&t,0x1000,&c));f=raw(&c);f.gpr[0]=S847_CALL_EXIT;
 CHECK(rt863_coop_commit(&bank,&d,&t,0x1000,2,&f,va,0)==PS_OK);CHECK(!memcmp(&bank.slots[1].context,&zero,sizeof zero)&&pr_find(&p,pid[1])->state==PR_EXITED&&!ws_find(&w,win[1]));
 CHECK(rt862_bank_release(&bank,&d,0x1000,0,pid[0],101,0x2000)==PS_OK);CHECK(rt862_bank_release(&bank,&d,0x1000,1,pid[1],102,0x3000)==PS_OK);CHECK(!pr_snapshot(&p,0,0,true)&&!ws_snapshot(&w,0,0)&&d.dispatcher.dispatches==130);
 printf("S863 cooperative bank host: %u positive / %u refusals PASS; 130 dispatches\n",pos,neg);
}
