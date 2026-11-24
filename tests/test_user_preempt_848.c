#include <styleos/user_preempt_848.h>
#include <styleos/user_copy_772.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned n;
#define C(x) do{assert(x);++n;}while(0)
static struct s847_raw frame(struct up_context c){struct s847_raw f={.vector=0x50,.rip=c.pc,.rsp=c.sp,.cs=c.cs,.ss=c.ss,.rflags=c.status};for(unsigned i=0;i<15;i++)f.gpr[i]=c.gpr[i];return f;}
static void reject(struct s847_control *c,struct s847_raw f,struct s848_irq_evidence e){struct s847_control old=*c;struct s847_raw before=f;C(!s848_timer_suspend(c,&f,&e));C(!memcmp(c,&old,sizeof old));C(!memcmp(&f,&before,sizeof f));}
int main(void){struct s847_control c={0};struct up_context ctx;unsigned id;struct s847_decision d;const uint64_t va=S846_IST+4096-16-sizeof(struct s847_raw);
 C(s847_init(&c,0x1000,0x2000,0x3000,0x5b,0x63));C(s848_user_dispatch(&c,0x1000,&id,&ctx)&&id==0&&ctx.status==0x202);
 struct s847_raw f=frame(ctx);struct s848_irq_evidence e={.root=0x2000,.frame_va=va,.live_flags=2,.isr=0x10000};
 for(unsigned i=0;i<15;i++)f.gpr[i]=0x848000+i;
 f.rip+=8;f.rsp-=8;f.rflags|=0x41;
 struct s847_raw bad=f;bad.rflags&=~0x200u;reject(&c,bad,e);bad=f;bad.rflags|=0x3000;reject(&c,bad,e);bad=f;bad.vector=128;reject(&c,bad,e);bad=f;bad.error=1;reject(&c,bad,e);bad=f;bad.cs=0x38;reject(&c,bad,e);bad=f;bad.ss=0x10;reject(&c,bad,e);bad=f;bad.rip=S846_DATA;reject(&c,bad,e);bad=f;bad.rsp=S846_STACK-1;reject(&c,bad,e);
 struct s848_irq_evidence wrong=e;wrong.root=0x3000;reject(&c,f,wrong);wrong=e;wrong.frame_va+=8;reject(&c,f,wrong);wrong=e;wrong.live_flags|=0x200;reject(&c,f,wrong);wrong=e;wrong.isr=0;reject(&c,f,wrong);
 struct s847_raw prior=f;C(s848_timer_suspend(&c,&f,&e));C(!memcmp(&f,&prior,sizeof f));C(c.current==-1&&c.slot[0].state==S847_READY);C(!s848_user_dispatch(&c,0x2000,&id,&ctx));
 C(s848_user_dispatch(&c,0x1000,&id,&ctx)&&id==1&&ctx.status==0x202);f=frame(ctx);e.root=0x3000;
 for(unsigned i=0;i<15;i++)f.gpr[i]=0x848100+i;
 C(s848_timer_suspend(&c,&f,&e));C(s848_user_dispatch(&c,0x1000,&id,&ctx)&&id==0);C(ctx.pc==prior.rip&&ctx.sp==prior.rsp&&ctx.status==prior.rflags);for(unsigned i=0;i<15;i++)C(ctx.gpr[i]==prior.gpr[i]);
 f=frame(ctx);f.vector=14;f.error=5;struct s847_control before=c;d=(struct s847_decision){S847_EXIT,9};struct s847_decision old=d;C(!s848_user_trap(&c,&f,va,0x1000,0x2000,2,&d));C(!memcmp(&c,&before,sizeof c)&&!memcmp(&d,&old,sizeof d));
 C(s848_user_trap(&c,&f,va+3*4096,0x1000,0x2000,2,&d)&&d.action==S847_FAULT);C(c.slot[0].state==S847_FAULTED);
 for(unsigned j=0;j<32;j++){C(s848_user_dispatch(&c,0x1000,&id,&ctx)&&id==1);for(unsigned i=0;i<15;i++)C(ctx.gpr[i]==0x848100+i);f=frame(ctx);C(s848_timer_suspend(&c,&f,&e));C(c.slot[0].state==S847_FAULTED);}
 C(s848_user_dispatch(&c,0x1000,&id,&ctx)&&id==1);f=frame(ctx);f.vector=128;f.gpr[0]=S847_CALL_RANGE;f.gpr[1]=S846_DATA;f.gpr[2]=8;prior=f;C(s848_user_trap(&c,&f,va,0,0x3000,2,&d)&&d.action==S847_RETURN&&d.result==UC772_OK);prior.gpr[0]=UC772_OK;C(!memcmp(&f,&prior,sizeof f));
 f.gpr[0]=S847_CALL_EXIT;C(s848_user_trap(&c,&f,va,0,0x3000,2,&d)&&d.action==S847_EXIT);C(!s848_user_dispatch(&c,0x1000,&id,&ctx));
 printf("S848 user timer context model: %u PASS\n",n);return 0;}
