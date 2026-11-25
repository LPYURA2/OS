#include <styleos/runtime_entry_858.h>
#include <styleos/runtime_tick_860.h>
#include <styleos/user_session_runtime.h>
#include <styleos/native_window_channel.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define B(n) (UINT64_C(1)<<(n))
static unsigned refused;
static struct rt858_expected e={0x1000,0x2000,0x3000,0x4000,0x5390,0x15390,0x57,0x48,0x38};
static struct rt858_live v={0x2000,0x3000,0x4000,0x14000,B(31)|B(16)|1,B(5),B(10)|B(11),2,0x57,4095,0x48,0x38};
static void reject(const struct rt858_live *x){
 const struct dpr_service *d=dpr_global_state();struct dpr_service before=*d;
 struct pr_table p=*pr_global();struct ws_service w=*ws_global();
 assert(!rt861_dispatcher(x));assert(!memcmp(d,&before,sizeof before)&&!memcmp(&p,pr_global(),sizeof p)&&!memcmp(&w,ws_global(),sizeof w));refused++;
}
int main(void){
 assert(!rt861_dispatcher(&v));assert(!dpr_global_state());
 assert(rt858_start(&e,&v)&&rt858_ready(e.root));
 struct dpr_service *d=rt861_dispatcher(&v);assert(d&&d==dpr_global_state()&&d==dpr_tick_global_860());
 const struct usr_runtime *u=usr_global();const struct nwc_service *n=nwc_global();
 uint32_t a,b,wa,wb,pid;unsigned slot;
 assert(dpr_admit(d,0,101,0x10000,0,"A.tea",&a)==PS_OK);
 assert(dpr_admit(d,1,102,0x20000,0,"B.tea",&b)==PS_OK);
 assert(ws_open_app(ws_global(),pr_global(),a,"A",(struct ws_bounds){0,0,400,300},&wa)==WS_OK);
 assert(ws_open_app(ws_global(),pr_global(),b,"B",(struct ws_bounds){0,0,400,300},&wb)==WS_OK);
 assert(ws_set_minimized(ws_global(),a,wa,true)==WS_OK);
 assert(!rt858_ready(e.root)&&rt861_dispatcher(&v)==d);
 assert(!rt858_start(&e,&v)&&dpr_global_start(e.root)==DPR_ALREADY_STARTED);
 #define MUT(field,width) do{for(unsigned i=0;i<(width);i++){struct rt858_live x=v;x.field^=B(i);reject(&x);}}while(0)
 MUT(root,64);MUT(gdt,64);MUT(idt,64);MUT(gdt_limit,16);MUT(idt_limit,16);MUT(tr,16);MUT(cs,16);
 reject(0);struct rt858_live x=v;x.rflags|=B(9);reject(&x);x=v;x.rsp=e.stack_hi;reject(&x);x=v;x.cr0^=B(16);reject(&x);
 for(unsigned i=0;i<64;i++){
  d->dispatcher.slots[0].handle^=B(i);reject(&v);d->dispatcher.slots[0].handle^=B(i);
  d->dispatcher.slots[1].user_root^=B(i);reject(&v);d->dispatcher.slots[1].user_root^=B(i);
 }
 unsigned old=d->dispatcher.next;d->dispatcher.next=PS_MAX_SLOTS;reject(&v);d->dispatcher.next=old;
 struct rt860_bounds bounds={{0x10000000,0x10001000},{0x11000000,0x11001000},{0x400000003000,0x400000004000},0x5b,0x63};
 struct s847_raw saved;
 for(unsigned step=0;step<8;step++){
  assert(rt861_dispatcher(&v)==d);assert(dpr_dispatch(d,&slot,&pid)==PS_OK&&slot==step%2&&pid==(slot?b:a));reject(&v);
  struct rt860_tick t={.slot=slot,.dispatch=d->dispatcher.dispatches,.pid=pid,.handle=slot?102:101,.child_root=slot?0x20000:0x10000,.safe_root=e.root,.frame_va=bounds.ist.end-16-sizeof t.frame,.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,.frame={.vector=0x50,.rip=bounds.code.start+2,.rsp=bounds.stack.end,.cs=bounds.cs,.ss=bounds.ss,.rflags=0x202}};
  assert(dpr_tick_860(d,&bounds,&t,&saved)==PS_OK&&rt861_dispatcher(&v)==d);
  assert(dpr_tick_860(d,&bounds,&t,&saved)!=PS_OK&&rt861_dispatcher(&v)==d);
 }
 assert(pr_find(pr_global(),a)->cpu_ticks==4&&pr_find(pr_global(),b)->cpu_ticks==4);
 assert(ws_find(ws_global(),wa)->minimized&&ws_find(ws_global(),wb));
 for(unsigned i=0;i<2;i++){
  assert(dpr_dispatch(d,&slot,&pid)==PS_OK&&slot==i);
  assert(dpr_observe(d,slot,slot?102:101,slot?0x20000:0x10000,e.root,slot?PS_EXIT:PS_FAULT)==PS_OK);
  assert(rt861_dispatcher(&v)==d);assert(dpr_release(d,slot,slot?102:101)==PS_OK);
 }
 assert(rt861_dispatcher(&v)==d&&d->dispatcher.dispatches==10&&!rt858_ready(e.root));
 assert(!pr_snapshot(pr_global(),0,0,true)&&!ws_snapshot(ws_global(),0,0)&&usr_global()==u&&nwc_global()==n);
 printf("S861 HOST ONLY: same singleton, 8 synthetic ticks, 10 dispatches, %u refusals, cleanup PASS\n",refused);
}
