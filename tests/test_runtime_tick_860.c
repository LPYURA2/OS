#include <styleos/runtime_tick_860.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct pr_table p,oldp;
static struct ws_service w,oldw;
static struct dpr_service s,olds;
static struct s847_raw saved,oldframe;
static struct rt860_bounds b={{0x10000000,0x10001000},{0x11000000,0x11001000},{0x400000003000,0x400000004000},0x5b,0x63};
static struct rt860_tick tick;
static unsigned refused;
static void snapshot(void){oldp=p;oldw=w;olds=s;oldframe=saved;}
static void unchanged(void){assert(!memcmp(&p,&oldp,sizeof p)&&!memcmp(&w,&oldw,sizeof w)&&!memcmp(&s,&olds,sizeof s)&&!memcmp(&saved,&oldframe,sizeof saved));}
static void reject(const struct rt860_bounds *bounds,const struct rt860_tick *t){snapshot();assert(dpr_tick_860(&s,bounds,t,&saved)!=PS_OK);unchanged();refused++;}
int main(void){
 uint32_t a,bpid,win,other,pid;unsigned slot;
 pr_init(&p);ws_init(&w);assert(dpr_start(&s,&p,&w,0x1000)==DPR_OK);
 assert(dpr_admit(&s,0,101,0x2000,0,"A.tea",&a)==PS_OK);
 assert(dpr_admit(&s,1,102,0x3000,0,"B.tea",&bpid)==PS_OK);
 assert(ws_open_app(&w,&p,a,"A",(struct ws_bounds){0,0,400,300},&win)==WS_OK);
 assert(ws_open_app(&w,&p,bpid,"B",(struct ws_bounds){0,0,400,300},&other)==WS_OK);
 assert(ws_set_minimized(&w,a,win,true)==WS_OK);
 assert(dpr_dispatch(&s,&slot,&pid)==PS_OK&&slot==0&&pid==a);
 assert(pr_account(&p,a,101,7,8192)==PR_OK);
 tick=(struct rt860_tick){.slot=0,.dispatch=s.dispatcher.dispatches,.pid=a,.handle=101,.child_root=0x2000,.safe_root=0x1000,.frame_va=b.ist.end-sizeof tick.frame,.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,.frame={.gpr={ [0]=0xaabb,[14]=0x1234 },.vector=0x50,.rip=b.code.start+2,.rsp=b.stack.end,.cs=b.cs,.ss=b.ss,.rflags=0x202}};
 memset(&saved,0xa5,sizeof saved);
 #define MUT(member,width) do {for(unsigned i=0;i<(width);i++){struct rt860_tick t=tick;t.member^=UINT64_C(1)<<i;reject(&b,&t);}}while(0)
 MUT(slot,32);MUT(dispatch,32);MUT(pid,32);MUT(handle,64);MUT(child_root,64);MUT(safe_root,64);MUT(frame_va,64);MUT(vector,64);MUT(isr,64);MUT(eoi,64);MUT(frame.vector,64);MUT(frame.error,64);MUT(frame.cs,64);MUT(frame.ss,64);
 for(unsigned i=0;i<64;i++){uint64_t bit=UINT64_C(1)<<i;struct rt860_tick t=tick;if(bit==2||!(bit&0x8d7)){t.safe_flags^=bit;reject(&b,&t);}t=tick;if(bit==2||bit==0x200||!(bit&0xad7)){t.frame.rflags^=bit;reject(&b,&t);}}
 struct rt860_tick bad=tick;bad.frame.rip=b.code.end;reject(&b,&bad);bad=tick;bad.frame.rsp=b.stack.start;reject(&b,&bad);bad.frame.rsp=b.stack.end+1;reject(&b,&bad);
 struct rt860_bounds wrong=b;wrong.ist=b.code;reject(&wrong,&tick);wrong=b;wrong.cs=0x38;reject(&wrong,&tick);wrong=b;wrong.ss=wrong.cs;reject(&wrong,&tick);wrong=b;wrong.stack.start++;reject(&wrong,&tick);
 reject(0,&tick);reject(&b,0);snapshot();assert(dpr_tick_860(&s,&b,&tick,0)==PS_ARGUMENT);unchanged();refused++;
 /* Both exhaustion paths must refuse without updating frame, registry or windows. */
 uint32_t serial=p.change_serial;p.change_serial=UINT32_MAX;reject(&b,&tick);p.change_serial=serial;
 struct pr_entry *entry=0;for(unsigned i=0;i<PR_MAX_PROCESSES;i++)if(p.processes[i].pid==a)entry=&p.processes[i];assert(entry);entry->cpu_ticks=UINT64_MAX;reject(&b,&tick);entry->cpu_ticks=7;
 snapshot();assert(dpr_tick_860(&s,&b,&tick,&saved)==PS_OK);assert(!memcmp(&saved,&tick.frame,sizeof saved));assert(pr_find(&p,a)->state==PR_READY&&pr_find(&p,a)->cpu_ticks==8&&pr_find(&p,a)->resident_bytes==8192&&p.change_serial==serial+1&&s.dispatcher.current==-1);assert(!memcmp(&w,&oldw,sizeof w));
 reject(&b,&tick);assert(dpr_dispatch(&s,&slot,&pid)==PS_OK&&slot==1&&pid==bpid);reject(&b,&tick);
 struct rt860_tick bt=tick;bt.slot=1;bt.dispatch=s.dispatcher.dispatches;bt.pid=bpid;bt.handle=102;bt.child_root=0x3000;bt.frame.gpr[0]=0xbeef;
 assert(dpr_tick_860(&s,&b,&bt,&saved)==PS_OK&&pr_find(&p,bpid)->cpu_ticks==1&&saved.gpr[0]==0xbeef);
 assert(dpr_dispatch(&s,&slot,&pid)==PS_OK&&slot==0&&pid==a);reject(&b,&tick);tick.dispatch=s.dispatcher.dispatches;
 assert(dpr_tick_860(&s,&b,&tick,&saved)==PS_OK&&pr_find(&p,a)->cpu_ticks==9);assert(ws_find(&w,win)&&ws_find(&w,win)->minimized&&ws_find(&w,other));
 assert(dpr_dispatch(&s,&slot,&pid)==PS_OK&&pid==bpid);assert(dpr_observe(&s,slot,102,0x3000,0x1000,PS_FAULT)==PS_OK&&!ws_find(&w,other)&&ws_find(&w,win));assert(dpr_release(&s,1,102)==PS_OK);
 assert(dpr_dispatch(&s,&slot,&pid)==PS_OK&&pid==a);assert(dpr_observe(&s,slot,101,0x2000,0x1000,PS_EXIT)==PS_OK&&!ws_find(&w,win));assert(dpr_release(&s,0,101)==PS_OK);
 assert(!pr_snapshot(&p,0,0,true)&&!ws_snapshot(&w,0,0));
 puts("Tick sequence A/B/A, stale ticket, overflow, window retention and terminal cleanup PASS (host model only)");printf("%u refusal cases; state and output unchanged PASS\n",refused);
 return 0;
}
