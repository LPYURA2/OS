#include <styleos/runtime_bank_862.h>
#include <styleos/physical_arena_844.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank bank;
static struct dpr_service d;
static struct pr_table p;
static struct ws_service w;
static struct s844_arena arena;
static _Alignas(4096) unsigned char pages[S844_BYTES];
static unsigned good,bad;
#define CHECK(x) do{assert(x);good++;}while(0)
#define REFUSE(x) do{struct rt862_bank b0=bank;struct dpr_service d0=d;struct pr_table p0=p;struct ws_service w0=w;struct s844_arena a0=arena;assert(!(x));assert(!memcmp(&bank,&b0,sizeof bank)&&!memcmp(&d,&d0,sizeof d)&&!memcmp(&p,&p0,sizeof p)&&!memcmp(&w,&w0,sizeof w)&&!memcmp(&arena,&a0,sizeof arena));bad++;}while(0)
static bool clean(uint64_t pa,size_t n){const unsigned char *b=(const void *)(uintptr_t)pa;while(n--)if(*b++)return false;return true;}
static struct s847_raw frame(const struct up_context *c){struct s847_raw f={.rip=c->pc,.rsp=c->sp,.cs=c->cs,.ss=c->ss,.rflags=c->status};for(unsigned j=0;j<15;j++)f.gpr[j]=c->gpr[j];return f;}
int main(void){
 pr_init(&p);ws_init(&w);CHECK(dpr_start(&d,&p,&w,0x1000)==DPR_OK);CHECK(s844_init(&arena,(uintptr_t)pages));
 struct s844_lease old[2]={{0}};struct rt862_selection previous[2]={{0}};unsigned total=0;
 for(unsigned round=0;round<64;round++){
  CHECK(!pr_snapshot(&p,0,0,true)&&!ws_snapshot(&w,0,0)&&d.dispatcher.dispatches==total);
  CHECK(rt862_bank_open(&bank,&d,0x1000,0x5b,0x63));REFUSE(rt862_bank_open(&bank,&d,0x1000,0x5b,0x63));
  struct s844_lease l[2];uint32_t pid[2];unsigned used;
  for(unsigned i=0;i<2;i++){
   CHECK(s844_acquire(&arena,0x8620+i,3,&l[i])&&clean(l[i].pa,12288));
   CHECK(l[i].generation==2*round+i+1&&l[i].pa==(uintptr_t)pages+12288*i);
   if(round){REFUSE(s844_release(&arena,old[i].owner,old[i]));CHECK(clean(l[i].pa,12288));}
   CHECK(rt862_bank_admit(&bank,&d,0x1000,i,l[i].generation,0x2000+4096*i,i?"B":"A",&pid[i])==PS_OK&&pid[i]==2*round+i+1);
   uint32_t win;CHECK(ws_open_app(&w,&p,pid[i],"test",(struct ws_bounds){0,0,400,300},&win)==WS_OK&&win==pid[i]);
   struct rt862_bank empty={0};REFUSE(rt862_bank_open(&empty,&d,0x1000,0x5b,0x63));CHECK(!empty.safe);
   memset((void *)(uintptr_t)l[i].pa,0xa5+i,12288);
  }
  unsigned ticks[2]={0},budget[2]={round%4+1,(round/4)%4+1};
  for(unsigned pass=0;;pass++){
   unsigned id;uint32_t chosen;enum ps_status status=dpr_dispatch(&d,&id,&chosen);
   if(status==PS_EMPTY)break;
   CHECK(status==PS_OK&&id<2&&chosen==pid[id]&&pass<12&&d.dispatcher.dispatches==++total);
   struct rt862_selection t={id,total,chosen,l[id].generation,0x2000+4096*id,0x1000};struct up_context c={0};
   if(round){struct rt862_selection stale=t;stale.pid=previous[id].pid;REFUSE(rt862_bank_enter(&bank,&d,&stale,0x1000,&c));stale=t;stale.handle=previous[id].handle;REFUSE(rt862_bank_enter(&bank,&d,&stale,0x1000,&c));REFUSE(rt862_bank_enter(&bank,&d,&previous[id],0x1000,&c));CHECK(!c.pc);}
   CHECK(rt862_bank_enter(&bank,&d,&t,0x1000,&c)&&c.gpr[0]==ticks[id]);
   struct s847_raw f=frame(&c);
   if(ticks[id]<budget[id]){
    f.vector=0x50;f.gpr[0]++;struct rt860_tick receipt={.slot=id,.dispatch=total,.pid=chosen,.handle=t.handle,.child_root=t.child_root,.safe_root=t.safe_root,.frame_va=S846_IST+4096-16-sizeof f,.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,.frame=f};
    CHECK(rt862_bank_tick(&bank,&d,&t,0x1000,&receipt)==PS_OK);ticks[id]++;CHECK(pr_find(&p,chosen)->cpu_ticks==ticks[id]);REFUSE(rt862_bank_tick(&bank,&d,&t,0x1000,&receipt)==PS_OK);
   }else{
    f.vector=128;f.gpr[0]=S847_CALL_EXIT;
    CHECK(rt862_bank_finish(&bank,&d,&t,0x1000,2,&f,S846_IST+4096-16-sizeof f,0)==PS_OK);CHECK(pr_find(&p,chosen)->state==PR_EXITED);
    CHECK(s844_release(&arena,l[id].owner,l[id])&&clean(l[id].pa,12288));CHECK(rt862_bank_release(&bank,&d,0x1000,id,pid[id],l[id].generation,t.child_root)==PS_OK);
    previous[id]=t;old[id]=l[id];
   }
  }
  CHECK(ticks[0]==budget[0]&&ticks[1]==budget[1]&&s844_count(&arena,&used)&&!used&&clean((uintptr_t)pages,sizeof pages));CHECK(!pr_snapshot(&p,0,0,true)&&!ws_snapshot(&w,0,0));
  /* Architecture owns this object and has already released/scrubbed both slots. */
  memset(&bank,0,sizeof bank);
 }
 CHECK(p.next_pid==129&&w.next_id==129&&arena.next==129&&total==448);
 printf("S862 recycle HOST ONLY: %u positive / %u refusals; 64 rounds, 128 leases/PIDs, 448 dispatches PASS\n",good,bad);return 0;
}
