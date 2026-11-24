#include <styleos/timer_context_843.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) unsigned char ist[4096];
static struct s843_scheduler s;
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static void refusal(struct s843_frame *f,uint64_t root){struct s843_scheduler old=s;struct s843_frame before=*f;CHECK(!s843_tick(&s,f,root));CHECK(!memcmp(&old,&s,sizeof s));CHECK(!memcmp(&before,f,sizeof *f));}
int main(void){
 const struct s843_task t[2]={{{0x100000,0x101000},{0x200000,0x204000},0x100100},{{0x110000,0x111000},{0x210000,0x214000},0x110100}};
 struct boot_range irq={(uintptr_t)ist,(uintptr_t)ist+sizeof ist};
 struct s843_frame *f=(void*)(ist+4096-sizeof *f);struct s843_frame outside;
 CHECK(s843_init(&s,t,irq,0x300000,8,16));CHECK(!s843_first(&s,&outside));CHECK(s843_first(&s,f));CHECK(f->rip==t[0].entry&&s.control.current==0);
 /* An IRQ without an instruction of progress is not a switch. */
 const uint64_t prev[2]={0,0};uint64_t now[2]={0,0};
 struct s843_scheduler unchanged=s;struct s843_frame same=*f;
 CHECK(s843_irq_progress(&s,f,0x300000,prev,now,0)==S843_IRQ_DEFER);
 CHECK(s843_irq_progress(&s,f,0x300000,prev,now,15)==S843_IRQ_DEFER);
 CHECK(s843_irq_progress(&s,f,0x300000,prev,now,16)==S843_IRQ_BAD);
 now[0]=1;CHECK(s843_irq_progress(&s,f,0x300000,prev,now,16)==S843_IRQ_SWITCH);
 now[1]=1;CHECK(s843_irq_progress(&s,f,0x300000,prev,now,0)==S843_IRQ_BAD);
 now[0]=now[1]=0;CHECK(s843_irq_progress(&s,f,0x301000,prev,now,0)==S843_IRQ_BAD);
 CHECK(s843_irq_progress(&s,&outside,0x300000,prev,now,0)==S843_IRQ_BAD);
 CHECK(!memcmp(&s,&unchanged,sizeof s)&&!memcmp(f,&same,sizeof *f));
 f->rflags|=0x3000;CHECK(s843_irq_progress(&s,f,0x300000,prev,now,0)==S843_IRQ_BAD);*f=same;
 f->rsp-=8;CHECK(s843_irq_progress(&s,f,0x300000,prev,now,0)==S843_IRQ_BAD);*f=same;
 f->rip=t[1].entry;CHECK(s843_irq_progress(&s,f,0x300000,prev,now,0)==S843_IRQ_BAD);*f=same;
 const uint64_t later[2]={1,0};CHECK(s843_irq_progress(&s,f,0x300000,later,now,0)==S843_IRQ_BAD);
 CHECK(s843_irq_progress(&s,f,0x300000,prev,now,17)==S843_IRQ_BAD);
 for(unsigned i=0;i<20;i++){
  unsigned cur=(unsigned)s.control.current;f->rip=t[cur].entry+8;f->rax=0xabcd+i;f->r15=0xdef0+i;f->rsp=t[cur].stack.end-64;
  CHECK(s843_tick(&s,f,0x300000));CHECK(s.control.current==(int)(1-cur)&&s.ticks==i+1);CHECK(s.saved[cur].rax==0xabcd+i&&s.saved[cur].r15==0xdef0+i);
  if(i)CHECK(f->rax==0xabcd+i-1&&f->r15==0xdef0+i-1&&f->rip==t[1-cur].entry+8&&f->rsp==t[1-cur].stack.end-64);
 }
 CHECK(s.control.slots[0].yields==10&&s.control.slots[1].yields==10&&s.control.dispatches==21);
 refusal(f,0x301000);outside=*f;refusal(&outside,0x300000);
 struct s843_frame good=*f;
 f->rip=t[1].entry;refusal(f,0x300000);*f=good;
 f->rsp=t[1].stack.end-16;refusal(f,0x300000);*f=good;
 f->rsp=t[0].stack.start+8;refusal(f,0x300000);*f=good;
 f->cs=11;refusal(f,0x300000);*f=good;
 f->ss=0;refusal(f,0x300000);*f=good;
 f->rflags|=0x3000;refusal(f,0x300000);*f=good;
 f->rflags&=~0x200;refusal(f,0x300000);*f=good;
 s.saved[1].rip=0;refusal(f,0x300000);s.saved[1].rip=t[1].entry;
 s.control.next=0;refusal(f,0x300000);s.control.next=1;
 s.control.dispatches=1;refusal(f,0x300000);s.control.dispatches=21;
 struct s843_scheduler before=s;struct s843_task bad[2]={t[0],t[1]};bad[1].stack=t[0].stack;
 CHECK(!s843_init(&s,bad,irq,0x300000,8,16));CHECK(!memcmp(&before,&s,sizeof s));
 CHECK(!s843_init(&s,t,irq,0x300001,8,16));CHECK(!memcmp(&before,&s,sizeof s));
 CHECK(!s843_first(&s,f));
 printf("S843 timer-context host assertions: %u PASS\n",checks);
}
