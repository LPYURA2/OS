#include <styleos/runtime_bank_862.h>
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
static struct s847_raw raw(const struct up_context *c){struct s847_raw r={.rip=c->pc,.rsp=c->sp,.cs=c->cs,.ss=c->ss,.rflags=c->status};for(unsigned i=0;i<15;i++)r.gpr[i]=c->gpr[i];return r;}
static struct rt860_tick receipt(const struct rt862_selection *t,const struct up_context *c){struct rt860_tick r={.slot=t->slot,.dispatch=t->dispatch,.pid=t->pid,.handle=t->handle,.child_root=t->child_root,.safe_root=t->safe_root,.frame_va=S846_IST+4096-16-sizeof(struct s847_raw),.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,.frame=raw(c)};r.frame.vector=0x50;r.frame.gpr[0]++;return r;}
static void tick(const struct rt862_selection *t,unsigned expected){struct up_context c={0};CHECK(rt862_bank_enter(&bank,&d,t,0x1000,&c));CHECK(c.status==0x202&&c.gpr[0]==expected);struct rt860_tick r=receipt(t,&c);CHECK(rt862_bank_tick(&bank,&d,t,0x1000,&r)==PS_OK);CHECK(pr_find(&p,t->pid)->state==PR_READY&&pr_find(&p,t->pid)->cpu_ticks==expected+1);CHECK(bank.slots[t->slot].context.status==2&&bank.slots[t->slot].context.gpr[0]==expected+1);REJECT(rt862_bank_tick(&bank,&d,t,0x1000,&r)==PS_OK);REJECT(rt862_bank_enter(&bank,&d,t,0x1000,&c));}
int main(void){
 uint32_t pid[2],win[2];pr_init(&p);ws_init(&w);CHECK(dpr_start(&d,&p,&w,0x1000)==DPR_OK);REJECT(rt862_bank_init(&bank,&d,0x1000,0x5b,0x63));
 for(unsigned i=0;i<2;i++){CHECK(dpr_admit(&d,i,101+i,0x2000+4096*i,0,i?"B":"A",&pid[i])==PS_OK);CHECK(ws_open_app(&w,&p,pid[i],i?"B":"A",(struct ws_bounds){0,0,400,300},&win[i])==WS_OK);}
 CHECK(ws_set_minimized(&w,pid[0],win[0],true)==WS_OK);CHECK(rt862_bank_init(&bank,&d,0x1000,0x5b,0x63));REJECT(rt862_bank_init(&bank,&d,0x1000,0x5b,0x63));
 struct rt862_selection t=select_slot(0);struct up_context c={0};CHECK(rt862_bank_enter(&bank,&d,&t,0x1000,&c));
 #define MUT(field,n) do{for(unsigned i=0;i<(n);i++){struct rt862_selection bad=t;bad.field^=UINT64_C(1)<<i;struct up_context before=c;REJECT(rt862_bank_enter(&bank,&d,&bad,0x1000,&c));assert(!memcmp(&before,&c,sizeof c));}}while(0)
 MUT(slot,32);MUT(dispatch,32);MUT(pid,32);MUT(handle,64);MUT(child_root,64);MUT(safe_root,64);
 REJECT(rt862_bank_enter(&bank,&d,&t,0x2000,&c));REJECT(rt862_bank_enter(&bank,&d,&t,0x1000,&bank.slots[0].context));
 struct rt860_tick r=receipt(&t,&c);
 #define RM(field,n) do{for(unsigned i=0;i<(n);i++){struct rt860_tick bad=r;bad.field^=UINT64_C(1)<<i;REJECT(rt862_bank_tick(&bank,&d,&t,0x1000,&bad)==PS_OK);}}while(0)
 RM(slot,32);RM(dispatch,32);RM(pid,32);RM(handle,64);RM(child_root,64);RM(safe_root,64);RM(frame_va,64);RM(vector,64);RM(isr,64);RM(eoi,64);RM(frame.cs,64);RM(frame.ss,64);RM(frame.vector,64);RM(frame.error,64);
 struct pr_entry *orphan=&p.processes[31];*orphan=p.processes[0];REJECT(rt862_bank_enter(&bank,&d,&t,0x1000,&c));memset(orphan,0,sizeof *orphan);
 tick(&t,0);struct rt862_selection old=t;
 for(unsigned i=1;i<128;i++){t=select_slot(i%2);REJECT(rt862_bank_enter(&bank,&d,&old,0x1000,&c));tick(&t,i/2);old=t;CHECK(ws_find(&w,win[0])->minimized);}
 t=select_slot(0);CHECK(rt862_bank_enter(&bank,&d,&t,0x1000,&c));struct s847_raw f=raw(&c);f.vector=14;f.error=5;uint64_t pf=S846_IST+4*4096-16-sizeof f;
 REJECT(rt862_bank_finish(&bank,&d,&t,0x2000,2,&f,pf,S846_IST)==PS_OK);REJECT(rt862_bank_finish(&bank,&d,&t,0x1000,0x202,&f,pf,S846_IST)==PS_OK);REJECT(rt862_bank_finish(&bank,&d,&t,0x1000,2,&f,pf-8,S846_IST)==PS_OK);
 CHECK(rt862_bank_finish(&bank,&d,&t,0x1000,2,&f,pf,S846_IST)==PS_OK);struct up_context z={0};CHECK(!memcmp(&bank.slots[0].context,&z,sizeof z));CHECK(pr_find(&p,pid[0])->state==PR_FAULTED&&!ws_find(&w,win[0])&&ws_find(&w,win[1]));REJECT(rt862_bank_finish(&bank,&d,&t,0x1000,2,&f,pf,S846_IST)==PS_OK);
 for(unsigned i=0;i<16;i++){t=select_slot(1);tick(&t,64+i);CHECK(pr_find(&p,pid[0])->state==PR_FAULTED);}
 t=select_slot(1);CHECK(rt862_bank_enter(&bank,&d,&t,0x1000,&c));f=raw(&c);f.vector=128;f.gpr[0]=S847_CALL_YIELD;uint64_t call=S846_IST+4096-16-sizeof f;
 REJECT(rt862_bank_finish(&bank,&d,&t,0x1000,2,&f,call,0)==PS_OK);f.gpr[0]=S847_CALL_EXIT;CHECK(rt862_bank_finish(&bank,&d,&t,0x1000,2,&f,call,0)==PS_OK);CHECK(!memcmp(&bank.slots[1].context,&z,sizeof z));
 CHECK(pr_find(&p,pid[0])->cpu_ticks==64&&pr_find(&p,pid[1])->cpu_ticks==80&&d.dispatcher.dispatches==146&&!ws_snapshot(&w,0,0));
 /* Host model only: real architecture must scrub FP and mapping/lease before release. */
 CHECK(dpr_release(&d,0,101)==PS_OK&&dpr_release(&d,1,102)==PS_OK&&!pr_snapshot(&p,0,0,true));
 printf("S862 bank HOST ONLY: %u positive / %u refusals PASS; 144 synthetic ticks, 146 dispatches; registry-only lifecycle\n",pos,neg);return 0;
}
