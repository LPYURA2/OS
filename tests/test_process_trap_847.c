#include <styleos/process_trap_847.h>
#include <styleos/user_copy_772.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned n;
#define C(x) do{assert(x);++n;}while(0)
static void reject(struct s847_control *c,struct up_context f,struct up_event e){
 struct s847_control before=*c;struct s847_decision d={S847_EXIT,42},old=d;
 C(!s847_trap(c,&f,&e,&d));C(!memcmp(c,&before,sizeof before));C(!memcmp(&d,&old,sizeof d));
}
int main(void){
 struct s847_control c={0};struct up_context f;unsigned id=99;struct s847_decision d;
 C(!s847_init(&c,0x1000,0x2000,0x2000,0x53,0x5b));C(s847_init(&c,0x1000,0x2000,0x3000,0x53,0x5b));C(!s847_init(&c,0x1000,0x2000,0x3000,0x53,0x5b));
 C(!s847_dispatch(&c,0x2000,&id,&f));C(id==99);C(s847_dispatch(&c,0x1000,&id,&f)&&id==0);C(f.cs==0x53&&f.ss==0x5b&&f.pc==S846_CODE&&f.sp==S846_STACK+4096);
 struct up_event e={.vector=128,.active_root=0x2000,.kernel_sp=S846_IST+4096-176};
 for(unsigned i=0;i<15;i++)f.gpr[i]=0x847000+i;
 f.gpr[0]=S847_CALL_RANGE;f.gpr[1]=S846_DATA+4096-32;f.gpr[2]=32;
 struct up_context bad=f;bad.cs=0x38;reject(&c,bad,e);bad=f;bad.ss=0x10;reject(&c,bad,e);bad=f;bad.status|=0x200;reject(&c,bad,e);bad=f;bad.pc=S846_DATA;reject(&c,bad,e);bad=f;bad.sp=S846_STACK-8;reject(&c,bad,e);bad=f;bad.gpr[15]=1;reject(&c,bad,e);
 struct up_event wrong=e;wrong.active_root=0x3000;reject(&c,f,wrong);wrong=e;wrong.kernel_sp=S846_IST-8;reject(&c,f,wrong);wrong=e;wrong.kernel_sp=S846_IST+4096-160;reject(&c,f,wrong);wrong=e;wrong.syndrome=1;reject(&c,f,wrong);
 C(s847_trap(&c,&f,&e,&d)&&d.action==S847_RETURN&&d.result==UC772_OK);C(c.current==0);
 for(unsigned i=1;i<15;i++)C(c.slot[0].context.gpr[i]==f.gpr[i]);
 f.gpr[2]=33;C(s847_trap(&c,&f,&e,&d)&&d.result==UC772_BAD);
 f.gpr[2]=32;f.gpr[1]++;C(s847_trap(&c,&f,&e,&d)&&d.result==UC772_RANGE);
 f.gpr[1]=UINT64_MAX;C(s847_trap(&c,&f,&e,&d)&&d.result==UC772_RANGE);
 f.gpr[1]=S846_CODE;C(s847_trap(&c,&f,&e,&d)&&d.result==UC772_RANGE);
 f.gpr[0]=UINT64_MAX;C(s847_trap(&c,&f,&e,&d)&&d.action==S847_RETURN&&d.result==UC772_BAD);
 f.gpr[0]=S847_CALL_YIELD;f.pc+=8;C(s847_trap(&c,&f,&e,&d)&&d.action==S847_YIELD);C(c.current==-1&&c.slot[0].state==S847_READY);
 C(!s847_dispatch(&c,0x2000,&id,&f));C(s847_dispatch(&c,0x1000,&id,&f)&&id==1);C(f.pc==S846_CODE&&f.gpr[14]==0);
 e.active_root=0x3000;f.gpr[0]=S847_CALL_YIELD;f.gpr[14]=0xdead;C(s847_trap(&c,&f,&e,&d)&&d.action==S847_YIELD);
 C(s847_dispatch(&c,0x1000,&id,&f)&&id==0);C(f.pc==S846_CODE+8&&f.gpr[14]==0x84700e);
 e=(struct up_event){.kind=UP_FAULT,.vector=14,.syndrome=5,.fault_va=0x1000,.active_root=0x2000,.kernel_sp=S846_IST+4*4096-176};
 wrong=e;wrong.syndrome=1;reject(&c,f,wrong);wrong=e;wrong.syndrome=13;reject(&c,f,wrong);wrong=e;wrong.vector=13;reject(&c,f,wrong);wrong=e;wrong.kernel_sp=S846_IST+4096-176;reject(&c,f,wrong);
 C(s847_trap(&c,&f,&e,&d)&&d.action==S847_FAULT&&c.slot[0].state==S847_FAULTED);
 C(s847_dispatch(&c,0x1000,&id,&f)&&id==1);C(f.gpr[14]==0xdead);
 e=(struct up_event){.vector=128,.active_root=0x3000,.kernel_sp=S846_IST+4096-176};f.gpr[0]=S847_CALL_EXIT;C(s847_trap(&c,&f,&e,&d)&&d.action==S847_EXIT);C(c.slot[1].state==S847_EXITED);C(!s847_dispatch(&c,0x1000,&id,&f));
 printf("S847 guarded trap/lifecycle model: %u PASS\n",n);
}
