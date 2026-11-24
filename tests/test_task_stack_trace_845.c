#include <styleos/task_stack_trace_845.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) unsigned char memory[S844_BYTES],ist[4096];
static struct s844_arena arena;
static struct s845_pair pair;
static struct s843_scheduler sched;
static struct s845_trace trace;
static unsigned n;
#define CHECK(x) do{assert(x);n++;}while(0)
static void reject(const struct s843_frame *f,uint64_t a,uint64_t b){struct s845_trace before=trace;CHECK(!s845_observe(&trace,&pair,&sched,f,a,b));CHECK(!memcmp(&trace,&before,sizeof trace));}
int main(void){
 const struct s845_code code[2]={{{0x100000,0x101000},0x100100},{{0x110000,0x111000},0x110100}};
 uint64_t root=0x300000,progress[2]={0};
 struct boot_range irq={(uintptr_t)ist,(uintptr_t)ist+sizeof ist};
 struct s843_frame *f=(void *)(ist+4096-16-sizeof *f);
 CHECK(s844_init(&arena,(uintptr_t)memory));CHECK(s845_prepare(&arena,&pair,&sched,code,irq,root,8,16));CHECK(s843_first(&sched,f));
 uint64_t *word[2]={(void *)(uintptr_t)(sched.task[0].stack.end-24),(void *)(uintptr_t)(sched.task[1].stack.end-24)};
 reject(f,0,0);
 for(unsigned i=0;i<16;i++){
  unsigned cur=i%2,other=1-cur;
  progress[cur]+=11;*word[cur]=progress[cur]+(i&1);
  uint64_t old=*word[other];*word[other]=old+1;reject(f,progress[0],progress[1]);*word[other]=old;
  uint64_t x=*word[cur];*word[cur]=progress[cur]+2;reject(f,progress[0],progress[1]);*word[cur]=x;
  struct s843_frame bad=*f;bad.rsp=sched.task[other].stack.end-16;reject(&bad,progress[0],progress[1]);
  CHECK(s845_observe(&trace,&pair,&sched,f,progress[0],progress[1]));CHECK(trace.ticks==i+1&&trace.count[cur]==x);
  reject(f,progress[0],progress[1]);CHECK(s843_tick(&sched,f,root));
 }
 CHECK(trace.count[0]==88&&trace.count[1]==89);reject(f,progress[0],progress[1]);
 CHECK(s845_release_terminal(&arena,&pair,&sched,f,(uintptr_t)f-128,root,2));
 unsigned used;CHECK(s844_count(&arena,&used)&&used==0);
 for(size_t i=0;i<sizeof memory;i++)assert(memory[i]==0);
 printf("S845 independent stack-write trace: %u PASS\n",n);return 0;
}
