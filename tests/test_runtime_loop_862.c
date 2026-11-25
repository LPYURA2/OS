/* Host model only: exercise the existing dispatcher/bank until PS_EMPTY. */
#include <styleos/runtime_bank_862.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,scenarios,total_ticks,total_dispatches;
#define CHECK(x) do{assert(x);checks++;}while(0)
static struct s847_raw raw(const struct up_context *c){struct s847_raw r={.rip=c->pc,.rsp=c->sp,.cs=c->cs,.ss=c->ss,.rflags=c->status};for(unsigned i=0;i<15;i++)r.gpr[i]=c->gpr[i];return r;}
static void trial(unsigned a,unsigned b){
 struct pr_table p;struct ws_service w;struct dpr_service d={0};struct rt862_bank bank={0};uint32_t pid[2],win[2];unsigned budgets[2]={a,b};unsigned seen[2]={0},completed=0,live=2,cursor=0;bool ended[2]={0};
 pr_init(&p);ws_init(&w);CHECK(dpr_start(&d,&p,&w,0x1000)==DPR_OK);
 for(unsigned i=0;i<2;i++){CHECK(dpr_admit(&d,i,101+i,0x2000+4096*i,0,i?"B":"A",&pid[i])==PS_OK);CHECK(ws_open_app(&w,&p,pid[i],i?"B":"A",(struct ws_bounds){0,0,400,300},&win[i])==WS_OK);}
 CHECK(rt862_bank_init(&bank,&d,0x1000,0x5b,0x63));
 for(;;){
  unsigned id=99;uint32_t chosen=0;enum ps_status selected=dpr_dispatch(&d,&id,&chosen);
  if(selected==PS_EMPTY){CHECK(!live&&id==99&&!chosen&&d.dispatcher.current==-1&&completed==a+b+2);break;}
  CHECK(selected==PS_OK&&completed<32&&id<2);
  /* Independent test oracle, not selection or terminal authority. */
  unsigned want=ended[cursor]?1-cursor:cursor;CHECK(id==want&&!ended[id]&&chosen==pid[id]);cursor=1-id;
  const struct pr_entry *running=pr_find(&p,chosen);CHECK(running&&running->state==PR_RUNNING&&running->cpu_ticks==seen[id]&&running->cpu_ticks<=budgets[id]);
  bool terminal=running->cpu_ticks==budgets[id];struct rt862_selection t={id,d.dispatcher.dispatches,chosen,d.dispatcher.slots[id].handle,d.dispatcher.slots[id].user_root,d.dispatcher.safe_root};struct up_context c={0};
  CHECK(rt862_bank_enter(&bank,&d,&t,0x1000,&c)&&c.gpr[0]==seen[id]&&c.status==0x202);
  struct s847_raw f=raw(&c);
  if(terminal){
   f.vector=id?128:14;f.error=id?0:5;f.gpr[0]=id?S847_CALL_EXIT:c.gpr[0];
   uint64_t va=S846_IST+(id?1u:4u)*4096-16-sizeof f;
   CHECK(rt862_bank_finish(&bank,&d,&t,0x1000,2,&f,va,id?0:S846_IST)==PS_OK);
   ended[id]=true;--live;struct up_context zero={0};CHECK(!memcmp(&bank.slots[id].context,&zero,sizeof zero));
   CHECK(pr_find(&p,chosen)->state==(id?PR_EXITED:PR_FAULTED)&&!ws_find(&w,win[id]));
  }else{
   f.vector=0x50;f.gpr[0]++;
   struct rt860_tick r={.slot=id,.dispatch=t.dispatch,.pid=chosen,.handle=t.handle,.child_root=t.child_root,.safe_root=t.safe_root,.frame_va=S846_IST+4096-16-sizeof f,.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,.frame=f};
   CHECK(rt862_bank_tick(&bank,&d,&t,0x1000,&r)==PS_OK);seen[id]++;total_ticks++;
   CHECK(pr_find(&p,chosen)->state==PR_READY&&pr_find(&p,chosen)->cpu_ticks==seen[id]&&ws_find(&w,win[id]));
  }
  CHECK(!rt862_bank_enter(&bank,&d,&t,0x1000,&c)&&d.dispatcher.current==-1&&ws_snapshot(&w,0,0)==live);completed++;total_dispatches++;
 }
 CHECK(seen[0]==a&&seen[1]==b&&ended[0]&&ended[1]);
 CHECK(dpr_release(&d,0,101)==PS_OK&&dpr_release(&d,1,102)==PS_OK&&!pr_snapshot(&p,0,0,true));scenarios++;
}
int main(void){for(unsigned a=0;a<=8;a++)for(unsigned b=0;b<=8;b++)trial(a,b);assert(scenarios==81&&total_ticks==648&&total_dispatches==810);printf("S862 loop HOST ONLY: %u checks, %u budgets, %u synthetic ticks, %u dispatches PASS\n",checks,scenarios,total_ticks,total_dispatches);return 0;}
