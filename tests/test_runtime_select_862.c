#include <styleos/runtime_select_862.h>
#include <styleos/user_preempt_848.h>
#include <styleos/runtime_tick_860.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct s847_control c;
static struct ps_dispatcher d;
static struct pr_table p;
static struct up_context out;
static unsigned good,bad;
#define C(x) do{assert(x);++good;}while(0)
static void reject(const struct rt862_selection *t,uint64_t root){
 struct s847_control cc=c;struct ps_dispatcher dd=d;struct pr_table pp=p;struct up_context oo=out;
 assert(!rt862_selected_context(&c,&d,t,root,&out));
 assert(!memcmp(&c,&cc,sizeof c)&&!memcmp(&d,&dd,sizeof d)&&!memcmp(&p,&pp,sizeof p)&&!memcmp(&out,&oo,sizeof out));++bad;
}
static struct rt862_selection select_one(unsigned expected){
 unsigned slot;uint32_t pid;C(ps_next(&d,&slot,&pid)==PS_OK&&slot==expected);
 return (struct rt862_selection){.slot=slot,.dispatch=d.dispatches,.pid=pid,.handle=d.slots[slot].handle,.child_root=d.slots[slot].user_root,.safe_root=d.safe_root};
}
static void bind(const struct rt862_selection *t){
 struct ps_dispatcher before=d;struct pr_table prior=p;unsigned cursor=c.next;
 C(rt862_selected_context(&c,&d,t,0x1000,&out));
 C(c.current==(int)t->slot&&out.status==0x202&&c.next==cursor);
 C(!memcmp(&d,&before,sizeof d)&&!memcmp(&p,&prior,sizeof p));
 reject(t,0x1000);
}
static void tick(const struct rt862_selection *t){
 struct s847_raw f={.vector=0x50,.rip=out.pc,.rsp=out.sp,.cs=out.cs,.ss=out.ss,.rflags=out.status};
 for(unsigned j=0;j<15;j++)f.gpr[j]=out.gpr[j]+1;
 struct s848_irq_evidence e={.root=t->child_root,.frame_va=S846_IST+4096-16-sizeof f,.live_flags=2,.isr=0x10000};
 C(s848_timer_suspend(&c,&f,&e));
 /* Synthetic S860 receipt: host model only, not hardware IRQ/EOI evidence. */
 const uint64_t before=pr_find(&p,t->pid)->cpu_ticks;
 const struct rt860_tick receipt={.slot=t->slot,.dispatch=t->dispatch,.pid=t->pid,.handle=t->handle,.child_root=t->child_root,.safe_root=t->safe_root,.frame_va=e.frame_va,.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,.frame=f};
 C(ps_tick_860(&d,&receipt)==PS_OK);
 C(pr_find(&p,t->pid)->cpu_ticks==before+1);
 C(ps_tick_860(&d,&receipt)!=PS_OK);
 C(c.slot[t->slot].context.gpr[0]==f.gpr[0]);reject(t,0x1000);
}
int main(void){
 uint32_t pid[2];pr_init(&p);C(ps_init(&d,&p,0x1000)==PS_OK);
 C(ps_register(&d,0,101,0x2000,0,"A",&pid[0])==PS_OK);C(ps_register(&d,1,102,0x3000,0,"B",&pid[1])==PS_OK);
 C(s847_init(&c,0x1000,0x2000,0x3000,0x5b,0x63));
 struct rt862_selection t=select_one(0);memset(&out,0xa5,sizeof out);
 #define MUT(field,n) do{for(unsigned k=0;k<(n);k++){struct rt862_selection x=t;x.field^=UINT64_C(1)<<k;reject(&x,0x1000);}}while(0)
 MUT(slot,32);MUT(dispatch,32);MUT(pid,32);MUT(handle,64);MUT(child_root,64);MUT(safe_root,64);
 reject(&t,0x2000);reject(0,0x1000);
 d.current=1;reject(&t,0x1000);d.current=0;
 c.slot[1].state=S847_EXITED;reject(&t,0x1000);c.slot[1].state=S847_READY;
 c.slot[1].root=0x4000;reject(&t,0x1000);c.slot[1].root=0x3000;
 bind(&t);tick(&t);
 struct rt862_selection stale=t;
 /* Legacy cursor remains A, but real dispatcher selects B. */
 for(unsigned i=1;i<128;i++){
  t=select_one(i%2);C(c.next==0);reject(&stale,0x1000);bind(&t);C(out.gpr[0]==i/2);tick(&t);stale=t;
 }
 t=select_one(0);bind(&t);struct s847_raw f={.vector=14,.error=5,.rip=out.pc,.rsp=out.sp,.cs=out.cs,.ss=out.ss,.rflags=out.status};
 struct s847_decision result;
 C(s848_user_trap(&c,&f,S846_IST+4*4096-16-sizeof f,S846_IST,t.child_root,2,&result)&&result.action==S847_FAULT);
 C(ps_accept(&d,t.slot,t.handle,t.child_root,0x1000,PS_FAULT)==PS_OK);
 for(unsigned i=0;i<16;i++){t=select_one(1);bind(&t);tick(&t);C(c.slot[0].state==S847_FAULTED);}
 t=select_one(1);bind(&t);f=(struct s847_raw){.vector=128,.rip=out.pc,.rsp=out.sp,.cs=out.cs,.ss=out.ss,.rflags=out.status,.gpr={[0]=S847_CALL_EXIT}};
 C(s848_user_trap(&c,&f,S846_IST+4096-16-sizeof f,0,t.child_root,2,&result)&&result.action==S847_EXIT);
 C(ps_accept(&d,t.slot,t.handle,t.child_root,0x1000,PS_EXIT)==PS_OK);reject(&t,0x1000);
 unsigned slot;uint32_t pid_out;C(ps_next(&d,&slot,&pid_out)==PS_EMPTY);
 C(pr_find(&p,pid[0])->cpu_ticks==64&&pr_find(&p,pid[1])->cpu_ticks==80&&d.dispatches==146);
 C(ps_reap(&d,0,101)==PS_OK&&ps_reap(&d,1,102)==PS_OK);C(!pr_snapshot(&p,0,0,true));
 printf("S862 selected-context HOST ONLY: %u positive / %u refusals PASS; 144 synthetic ticks; one dispatch authority\n",good,bad);
 return 0;
}
