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
static struct rt862_selection select_slot(unsigned want){unsigned i;uint32_t pid;CHECK(dpr_dispatch(&d,&i,&pid)==PS_OK&&i==want);return (struct rt862_selection){i,d.dispatcher.dispatches,pid,d.dispatcher.slots[i].handle,d.dispatcher.slots[i].user_root,d.dispatcher.safe_root};}
static struct s847_raw raw(const struct up_context *c){struct s847_raw r={.rip=c->pc,.rsp=c->sp,.cs=c->cs,.ss=c->ss,.rflags=c->status,.vector=128};for(unsigned i=0;i<15;i++)r.gpr[i]=c->gpr[i];return r;}
int main(void){
 uint32_t pid[2],win[2];pr_init(&p);ws_init(&w);CHECK(dpr_start(&d,&p,&w,0x1000)==DPR_OK);
 for(unsigned i=0;i<2;i++){CHECK(dpr_admit(&d,i,101+i,0x2000+4096*i,0,i?"B":"A",&pid[i])==PS_OK);CHECK(ws_open_app(&w,&p,pid[i],i?"B":"A",(struct ws_bounds){0,0,400,300},&win[i])==WS_OK);}
 CHECK(rt862_bank_init(&bank,&d,0x1000,0x5b,0x63));
 const uint64_t va=S846_IST+4096-16-sizeof(struct s847_raw);
 const uint64_t calls[]={S847_CALL_RANGE,S847_CALL_RANGE,S847_CALL_RANGE,S847_CALL_RANGE,999,S847_CALL_YIELD};
 const uint64_t ptr[]={S846_DATA,S846_DATA+4095,S846_DATA+4095,UINT64_MAX,0,0};
 const uint64_t len[]={32,1,2,1,0,0};
 const uint64_t want[]={UC772_OK,UC772_OK,UC772_RANGE,UC772_RANGE,UC772_BAD,0};
 for(unsigned i=0;i<12;i++){
  struct rt862_selection t=select_slot(i%2);struct up_context c={0};CHECK(rt862_bank_enter(&bank,&d,&t,0x1000,&c));
  if(i>=2)CHECK(c.gpr[0]==want[i/2-1]);
  struct s847_raw f=raw(&c);f.gpr[0]=calls[i/2];f.gpr[1]=ptr[i/2];f.gpr[2]=len[i/2];f.gpr[14]=i+42;f.rip=S846_CODE+i+1;
  REJECT(rt863_bank_syscall(&bank,&d,&t,0x2000,2,&f,va,0)==PS_OK);
  REJECT(rt863_bank_syscall(&bank,&d,&t,0x1000,0x202,&f,va,0)==PS_OK);
  REJECT(rt863_bank_syscall(&bank,&d,&t,0x1000,2,&f,va-8,0)==PS_OK);
  REJECT(rt863_bank_syscall(&bank,&d,&t,0x1000,2,&f,va,1)==PS_OK);
  struct s847_raw bad=f;bad.gpr[0]=S847_CALL_EXIT;REJECT(rt863_bank_syscall(&bank,&d,&t,0x1000,2,&bad,va,0)==PS_OK);
  bad=f;bad.rflags=2;REJECT(rt863_bank_syscall(&bank,&d,&t,0x1000,2,&bad,va,0)==PS_OK);
  bad=f;bad.cs^=1;REJECT(rt863_bank_syscall(&bank,&d,&t,0x1000,2,&bad,va,0)==PS_OK);
  bad=f;bad.vector=14;REJECT(rt863_bank_syscall(&bank,&d,&t,0x1000,2,&bad,va,0)==PS_OK);
  struct rt862_selection stale=t;stale.dispatch--;REJECT(rt863_bank_syscall(&bank,&d,&stale,0x1000,2,&f,va,0)==PS_OK);
  CHECK(rt863_bank_syscall(&bank,&d,&t,0x1000,2,&f,va,0)==PS_OK);
  CHECK(d.dispatcher.current==-1&&pr_find(&p,t.pid)->state==PR_READY&&pr_find(&p,t.pid)->cpu_ticks==0);
  CHECK(bank.slots[t.slot].context.gpr[0]==want[i/2]&&bank.slots[t.slot].context.gpr[14]==i+42&&bank.slots[t.slot].context.pc==f.rip&&bank.slots[t.slot].context.status==2);
  CHECK(ws_find(&w,win[0])&&ws_find(&w,win[1]));
  REJECT(rt863_bank_syscall(&bank,&d,&t,0x1000,2,&f,va,0)==PS_OK);
 }
 CHECK(d.dispatcher.dispatches==12&&pr_snapshot(&p,0,0,true)==2);
 printf("S863 syscall continuation: %u positive / %u refusals PASS (host only)\n",pos,neg);
}
