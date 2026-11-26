/* Host contract for ordinary native service failures and peer progress.
 * No hardware/guest claims: the receipt is a typed test input. */
#include <styleos/runtime_bank_862.h>
#include <styleos/user_copy_772.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank bank;
static struct dpr_service service;
static struct pr_table registry;
static struct ws_service windows;
static uint32_t pids[2],win[2];
static unsigned checks;
#define CHECK(x) do { assert(x); ++checks; } while(0)
static void slice(unsigned expected,uint64_t call,uint64_t address,uint64_t bytes,uint64_t result){
 unsigned slot=99;uint32_t pid=0;
 CHECK(dpr_dispatch(&service,&slot,&pid)==PS_OK&&slot==expected&&pid==pids[slot]);
 const unsigned peer=1-slot;
 const struct rt862_bank_slot peer_saved=bank.slots[peer];
 const struct pr_entry peer_process=*pr_find(&registry,pids[peer]);
 const struct ws_window peer_window=*ws_find(&windows,win[peer]);
 const struct ws_window own_window=*ws_find(&windows,win[slot]);
 const struct rt862_selection ticket={slot,service.dispatcher.dispatches,pid,101+slot,0x2000+4096*slot,0x1000};
 struct up_context ctx={0};
 CHECK(rt862_bank_enter(&bank,&service,&ticket,0x1000,&ctx));
 struct rt860_tick receipt={.slot=slot,.dispatch=ticket.dispatch,.pid=pid,
  .handle=ticket.handle,.child_root=ticket.child_root,.safe_root=0x1000,
  .frame_va=S846_IST+4096-16-sizeof(struct s847_raw),.vector=128,.safe_flags=2,
  .frame={.vector=128,.rip=ctx.pc,.rsp=ctx.sp,.cs=ctx.cs,.ss=ctx.ss,.rflags=ctx.status}};
 for(unsigned i=0;i<15;i++)receipt.frame.gpr[i]=1000+slot*100+i;
 receipt.frame.gpr[0]=call;receipt.frame.gpr[1]=address;receipt.frame.gpr[2]=bytes;
 CHECK(rt863_bank_event(&bank,&service,&ticket,0x1000,&receipt,0)==PS_OK);
 CHECK(service.dispatcher.current==-1&&pr_find(&registry,pid)->state==PR_READY);
 CHECK(bank.slots[slot].context.gpr[0]==result);
 for(unsigned i=1;i<15;i++)CHECK(bank.slots[slot].context.gpr[i]==receipt.frame.gpr[i]);
 CHECK(!memcmp(&peer_saved,&bank.slots[peer],sizeof peer_saved));
 CHECK(!memcmp(&peer_process,pr_find(&registry,pids[peer]),sizeof peer_process));
 CHECK(!memcmp(&peer_window,ws_find(&windows,win[peer]),sizeof peer_window));
 CHECK(!memcmp(&own_window,ws_find(&windows,win[slot]),sizeof own_window));
}
int main(void){
 pr_init(&registry);ws_init(&windows);
 CHECK(dpr_start(&service,&registry,&windows,0x1000)==DPR_OK);
 CHECK(rt862_bank_open(&bank,&service,0x1000,0x5b,0x63));
 for(unsigned i=0;i<2;i++){
  CHECK(rt862_bank_admit(&bank,&service,0x1000,i,101+i,0x2000+4096*i,i?"Peer":"Worker",&pids[i])==PS_OK);
  CHECK(ws_open_app(&windows,&registry,pids[i],i?"Peer":"Worker",(struct ws_bounds){0,0,200,100},&win[i])==WS_OK);
 }
 const struct {uint64_t call,address,bytes,result;} cases[]={
  {S847_CALL_RANGE,S846_DATA,0,UC772_BAD},
  {S847_CALL_RANGE,S846_DATA,UC772_MAX_COPY+1,UC772_BAD},
  {S847_CALL_RANGE,S846_DATA+UC772_PAGE-1,2,UC772_RANGE},
  {S847_CALL_RANGE,S846_DATA+UC772_PAGE,1,UC772_RANGE},
  {S847_CALL_RANGE,S846_DATA-1,1,UC772_RANGE},
  {S847_CALL_RANGE,S846_DATA+UC772_PAGE-UC772_MAX_COPY,UC772_MAX_COPY,UC772_OK},
  {S847_CALL_RANGE,S846_DATA,1,UC772_OK},
  {UINT64_MAX,0,0,UC772_BAD},
  {S847_CALL_YIELD,0,0,0}
 };
 for(unsigned round=0;round<3;round++){
  for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){
   slice(0,cases[i].call,cases[i].address,cases[i].bytes,cases[i].result);
   slice(1,S847_CALL_YIELD,0,0,0);
  }
 }
 CHECK(pr_snapshot(&registry,0,0,true)==2);
 CHECK(ws_snapshot(&windows,0,0)==2);
 CHECK(service.dispatcher.dispatches==54);
 for(unsigned i=0;i<2;i++){
  CHECK(rt865_bank_stop_ready(&bank,&service,0x1000,i,pids[i],101+i,0x2000+4096*i,true)==PS_OK);
  CHECK(rt862_bank_release(&bank,&service,0x1000,i,pids[i],101+i,0x2000+4096*i)==PS_OK);
 }
 CHECK(!pr_snapshot(&registry,0,0,true)&&!ws_snapshot(&windows,0,0));
 puts("Native ordinary API error and peer progress: 54 HOST receipt slices PASS");
 printf("Assertions: %u\n",checks);
 return 0;
}
