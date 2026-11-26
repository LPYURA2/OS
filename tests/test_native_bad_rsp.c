#include <styleos/runtime_bank_862.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank b,oldb;
static struct dpr_service d,oldd;
static struct pr_table p,oldp;
static struct ws_service w,oldw;
#define REFUSE(call) do{oldb=b;oldd=d;oldp=p;oldw=w;assert((call)==PS_EVIDENCE);assert(!memcmp(&oldb,&b,sizeof b)&&!memcmp(&oldd,&d,sizeof d)&&!memcmp(&oldp,&p,sizeof p)&&!memcmp(&oldw,&w,sizeof w));}while(0)
int main(void){
 for(unsigned i=0;i<3;i++){
  memset(&b,0,sizeof b);memset(&d,0,sizeof d);pr_init(&p);ws_init(&w);assert(dpr_start(&d,&p,&w,0x1000)==DPR_OK);assert(rt862_bank_open(&b,&d,0x1000,0x5b,0x63));
  uint32_t a,peer;assert(rt862_bank_admit(&b,&d,0x1000,0,11,0x2000,"bad",&a)==PS_OK);assert(rt862_bank_admit(&b,&d,0x1000,1,12,0x3000,"peer",&peer)==PS_OK);
  unsigned slot;uint32_t pid;assert(dpr_dispatch(&d,&slot,&pid)==PS_OK&&slot==0&&pid==a);
  struct rt862_selection t={slot,d.dispatcher.dispatches,pid,11,0x2000,0x1000};
  unsigned vector=i==0?6:i==1?128:0x50;
  struct rt860_tick r={.slot=slot,.dispatch=t.dispatch,.pid=pid,.handle=11,.child_root=0x2000,.safe_root=0x1000,.frame_va=S846_IST+4096-16-sizeof(struct s847_raw)+(i==0?3*4096:0),.vector=vector,.isr=i==2,.eoi=i==2,.safe_flags=2,.frame={.vector=vector,.rip=S846_CODE+16,.rsp=0x12000000,.cs=0x5b,.ss=0x63,.rflags=0x202}};
  struct rt860_tick bad=r;bad.frame_va+=8;REFUSE(rt863_bank_event(&b,&d,&t,0x1000,&bad,0));bad=r;bad.eoi^=1;REFUSE(rt863_bank_event(&b,&d,&t,0x1000,&bad,0));bad=r;bad.frame.cs=0x38;REFUSE(rt863_bank_event(&b,&d,&t,0x1000,&bad,0));
  assert(rt863_bank_event(&b,&d,&t,0x1000,&r,0)==PS_OK);struct up_context zero={0};assert(!memcmp(&b.slots[0].context,&zero,sizeof zero));assert(pr_find(&p,a)->state==PR_FAULTED&&pr_find(&p,peer)->state==PR_READY);assert(rt862_bank_release(&b,&d,0x1000,0,a,11,0x2000)==PS_OK);assert(dpr_dispatch(&d,&slot,&pid)==PS_OK&&slot==1&&pid==peer);
 }
 puts("NATIVE BAD RSP HOST PASS: UD, syscall, IRQ, peer survives; forged evidence rejected");
}
