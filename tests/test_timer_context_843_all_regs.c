#include <styleos/timer_context_843.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) unsigned char ist[4096];
static struct s843_scheduler s;
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static void seed(struct s843_frame *f,unsigned task,unsigned epoch){
 uint64_t pattern[15];for(unsigned i=0;i<15;i++)pattern[i]=UINT64_C(0x8430000000000000)|((uint64_t)task<<32)|((uint64_t)epoch<<8)|i;
 memcpy(f,pattern,sizeof pattern);
}
int main(void){
 const struct s843_task t[2]={{{0x100000,0x101000},{0x200000,0x204000},0x100100},{{0x110000,0x111000},{0x210000,0x214000},0x110100}};
 struct s843_frame *f=(void*)(ist+sizeof ist-sizeof *f);
 struct boot_range irq={(uintptr_t)ist,(uintptr_t)ist+sizeof ist};
 CHECK(s843_init(&s,t,irq,0x300000,8,16));
 struct s843_frame expected[2]={s.saved[0],s.saved[1]};
 for(unsigned i=0;i<2;i++){seed(&expected[i],i,0);s.saved[i]=expected[i];}
 CHECK(s843_first(&s,f));CHECK(!memcmp(f,&expected[0],sizeof *f));
 for(unsigned i=0;i<128;i++){
  unsigned cur=i%2,next=1-cur;
  CHECK(s.control.current==(int)cur);CHECK(!memcmp(f,&expected[cur],sizeof *f));
  seed(f,cur,i+1);f->rip=t[cur].entry+((i%8)*8);f->rsp=t[cur].stack.end-16-((i%8)*8);f->rflags=0x202|(i&1?0x40:0x81);
  expected[cur]=*f;
  CHECK(s843_tick(&s,f,0x300000));CHECK(!memcmp(f,&expected[next],sizeof *f));CHECK(!memcmp(&s.saved[cur],&expected[cur],sizeof *f));
 }
 CHECK(s.ticks==128&&s.control.dispatches==129&&s.control.slots[0].yields==64&&s.control.slots[1].yields==64);
 printf("S843 all-register frame host assertions: %u PASS\n",checks);
}
