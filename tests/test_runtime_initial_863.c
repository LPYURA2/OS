#include <styleos/runtime_bank_862.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank b;
static struct dpr_service d;
static struct pr_table p;
static struct ws_service w;
static unsigned refusals,admissions;
#define REFUSE(expr) do{struct rt862_bank b0=b;struct dpr_service d0=d;struct pr_table p0=p;struct ws_service w0=w;uint32_t old=pid;assert((expr)!=PS_OK);assert(pid==old&&!memcmp(&b,&b0,sizeof b)&&!memcmp(&d,&d0,sizeof d)&&!memcmp(&p,&p0,sizeof p)&&!memcmp(&w,&w0,sizeof w));++refusals;}while(0)
int main(void)
{
 pr_init(&p);ws_init(&w);
 assert(dpr_start(&d,&p,&w,0x1000)==DPR_OK);
 assert(rt862_bank_open(&b,&d,0x1000,0x5b,0x63));
 for(unsigned n=0;n<128;n++){
  unsigned slot=n%2;uint64_t root=0x2000+4096*slot,handle=1+n;
  struct up_context c={.pc=S846_CODE+1+n,.sp=S846_STACK+4096-16*n,.status=2,.cs=0x5b,.ss=0x63};
  for(unsigned i=0;i<15;i++)c.gpr[i]=(uint64_t)n*100+i;
  uint32_t pid=0xdeadbeefu;
  for(unsigned i=0;i<9;i++){
   struct up_context bad=c;
   switch(i){
    case 0:bad.pc=S846_CODE-1;break;
    case 1:bad.pc=S846_CODE+4096;break;
    case 2:bad.sp=S846_STACK;break;
    case 3:bad.sp++;break;
    case 4:bad.sp=S846_STACK+4096+16;break;
    case 5:bad.cs=8;break;
    case 6:bad.ss=8;break;
    case 7:bad.status=0x202;break;
    case 8:bad.gpr[15]=1;break;
   }
   REFUSE(rt863_bank_admit_initial(&b,&d,0x1000,slot,handle,root,"initial",&bad,&pid));
  }
  REFUSE(rt863_bank_admit_initial(&b,&d,0x3000,slot,handle,root,"initial",&c,&pid));
  REFUSE(rt863_bank_admit_initial(&b,&d,0x1000,slot,handle,root,"initial",0,&pid));
  REFUSE(rt863_bank_admit_initial(&b,&d,0x1000,slot,handle,root,"initial",&b.slots[slot].context,&pid));
  REFUSE(rt863_bank_admit_initial(&b,&d,0x1000,slot,handle,root,"initial",&c,(uint32_t *)(void *)&c));
  assert(rt863_bank_admit_initial(&b,&d,0x1000,slot,handle,root,"initial",&c,&pid)==PS_OK&&pid==n+1);
  ++admissions;
  assert(!memcmp(&c,&b.slots[slot].context,sizeof c));
  REFUSE(rt863_bank_admit_initial(&b,&d,0x1000,slot,handle,root,"initial",&c,&pid));
  unsigned chosen;uint32_t actual;assert(dpr_dispatch(&d,&chosen,&actual)==PS_OK&&chosen==slot&&actual==pid);
  struct rt862_selection t={slot,d.dispatcher.dispatches,pid,handle,root,0x1000};
  struct up_context got={0};assert(rt862_bank_enter(&b,&d,&t,0x1000,&got));c.status|=0x200;
  assert(!memcmp(&c,&got,sizeof c));
  REFUSE(rt863_bank_admit_initial(&b,&d,0x1000,1-slot,handle+1,0x5000,"initial",&c,&pid));
  struct s847_raw end={.vector=128,.rip=got.pc,.rsp=got.sp,.cs=got.cs,.ss=got.ss,.rflags=got.status,.gpr={[0]=S847_CALL_EXIT}};
  assert(rt862_bank_finish(&b,&d,&t,0x1000,2,&end,S846_IST+4096-16-sizeof end,0)==PS_OK);
  assert(rt862_bank_release(&b,&d,0x1000,slot,pid,handle,root)==PS_OK);
  assert(!pr_snapshot(&p,0,0,true)&&!ws_snapshot(&w,0,0));
 }
 printf("S863 initial context HOST ONLY: %u admissions, %u immutable refusals PASS\n",admissions,refusals);
}
