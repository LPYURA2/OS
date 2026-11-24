#include <styleos/task_stacks_845.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) unsigned char memory[S844_BYTES],ist[4096];
static struct s844_arena arena;
static struct s845_pair pair;
static struct s843_scheduler sched;
static unsigned n;
#define CHECK(x) do{assert(x);n++;}while(0)
static const struct s845_code code[2]={{{0x100000,0x101000},0x100100},{{0x110000,0x111000},0x110100}};
static bool zeros(const void *p,size_t n){const unsigned char *v=p;for(size_t i=0;i<n;i++)if(v[i])return false;return true;}
static void refusal(struct s843_frame *f,uint64_t rsp,uint64_t root,uint64_t flags){
 struct s844_arena a=arena;struct s845_pair p=pair;struct s843_scheduler s=sched;struct s843_frame old=*f;
 CHECK(!s845_release_terminal(&arena,&pair,&sched,f,rsp,root,flags));
 CHECK(!memcmp(&a,&arena,sizeof a)&&!memcmp(&p,&pair,sizeof p)&&!memcmp(&s,&sched,sizeof s)&&!memcmp(&old,f,sizeof old));
}
int main(void){
 uint64_t base=(uintptr_t)memory,root=0x300000;unsigned used;
 struct boot_range irq={(uintptr_t)ist,(uintptr_t)ist+4096};
 struct s843_frame *f=(void*)(ist+4096-sizeof *f-16);uint64_t rsp=(uintptr_t)f-128;
 CHECK(s844_init(&arena,base));memset(memory,0xcc,sizeof memory);
 struct s845_code bad[2]={code[0],code[1]};bad[0]=(struct s845_code){{base,base+4096},base};
 CHECK(!s845_prepare(&arena,&pair,&sched,bad,irq,root,8,16));CHECK(memory[0]==0xcc&&arena.next==1);
 CHECK(!s845_prepare(&arena,&pair,&sched,code,irq,root+1,8,16));
 CHECK(s845_prepare(&arena,&pair,&sched,code,irq,root,8,16));
 CHECK(pair.live&&sched.ready&&s844_count(&arena,&used)&&used==8);
 CHECK(pair.lease[0].pa==base&&pair.lease[1].pa==base+TASKS_STACK_BYTES);
 CHECK(zeros(memory,2*TASKS_STACK_BYTES)&&memory[2*TASKS_STACK_BYTES]==0xcc);
 CHECK(!s845_prepare(&arena,&pair,&sched,code,irq,root,8,16));
 CHECK(s843_first(&sched,f));refusal(f,rsp,root,2);
 for(unsigned i=0;i<16;i++){
  unsigned cur=i%2;CHECK(sched.control.current==(int)cur);
  CHECK(f->rsp==pair.lease[cur].pa+TASKS_STACK_BYTES-16);
  uint64_t value=0x84500000u+i;memcpy((void*)(uintptr_t)(f->rsp-8),&value,8);
  CHECK(s843_tick(&sched,f,root));
 }
 refusal(f,rsp,root,0x202);refusal(f,rsp,root+4096,2);refusal(f,base+1024,root,2);
 uint64_t gen=pair.lease[1].generation;pair.lease[1].generation++;refusal(f,rsp,root,2);pair.lease[1].generation=gen;
 CHECK(s845_release_terminal(&arena,&pair,&sched,f,rsp,root,2));
 CHECK(!pair.live&&!sched.ready&&zeros(f,sizeof *f));CHECK(s844_count(&arena,&used)&&used==0);
 CHECK(zeros(memory,2*TASKS_STACK_BYTES)&&memory[2*TASKS_STACK_BYTES]==0xcc);
 CHECK(!s843_tick(&sched,f,root));CHECK(!s845_release_terminal(&arena,&pair,&sched,f,rsp,root,2));
 CHECK(s845_prepare(&arena,&pair,&sched,code,irq,root,8,16));
 CHECK(pair.lease[0].generation==3&&pair.lease[1].generation==4);
 CHECK(!s844_release(&arena,S845_OWNER_A,(struct s844_lease){base,1,S845_OWNER_A,4}));
 printf("S845 leased task-stack model: %u PASS\n",n);return 0;
}
