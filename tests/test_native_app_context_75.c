#include <styleos/runtime_bank_862.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank bank,oldbank;
static struct dpr_service d,oldd;
static struct pr_table p,oldp;
static struct ws_service w,oldw;
static unsigned checks;
#define C(x) do{assert(x);++checks;}while(0)
#define REFUSE(x) do{oldbank=bank;oldd=d;oldp=p;oldw=w;C((x)==PS_EVIDENCE);C(!memcmp(&oldbank,&bank,sizeof bank)&&!memcmp(&oldd,&d,sizeof d)&&!memcmp(&oldp,&p,sizeof p)&&!memcmp(&oldw,&w,sizeof w));}while(0)
#define P UINT64_C(4096)
static const uint64_t va=S846_IST+P-16-sizeof(struct s847_raw);
static struct rt862_selection pick(unsigned slot,uint32_t pid){
 unsigned i;uint32_t id;C(dpr_dispatch(&d,&i,&id)==PS_OK&&i==slot&&id==pid);
 const struct ps_slot *s=&d.dispatcher.slots[i];
 return (struct rt862_selection){i,d.dispatcher.dispatches,id,s->handle,s->user_root,d.dispatcher.safe_root};
}
static struct rt860_tick receipt(struct rt862_selection t,struct up_context c,uint64_t vector){
 struct rt860_tick r={.slot=t.slot,.dispatch=t.dispatch,.pid=t.pid,.handle=t.handle,.child_root=t.child_root,
 .safe_root=t.safe_root,.frame_va=va,.vector=vector,.isr=vector==0x50,.eoi=vector==0x50,.safe_flags=2,
 .frame={.vector=vector,.rip=c.pc,.rsp=c.sp,.cs=c.cs,.ss=c.ss,.rflags=c.status}};
 for(unsigned i=0;i<15;++i)r.frame.gpr[i]=c.gpr[i];
 return r;
}
int main(void){
 pr_init(&p);ws_init(&w);C(dpr_start(&d,&p,&w,0x1000)==DPR_OK);
 C(rt862_bank_open(&bank,&d,0x1000,0x5b,0x63));
 /* Synthetic admission metadata only: actual PTE/lease/FP proof is a
  * separate architecture gate, not claimed by this model. */
 struct s844_lease lease={.pa=0x800000,.generation=101,.owner=S846_OWNER_A,.pages=7};
 struct s846_root child={.ready=true,.lease=lease,.built={.root=0x2000,.used=5,.error=PG_OK},
 .plan={.arch=PG_X86,.tables_pa=0x2000,.capacity=PG_TABLE_PAGES,.count=3,
 .maps={{S846_CODE,0x800000,2*P,PG_EXEC},{S846_DATA,0x802000,3*P,PG_WRITE},{S846_STACK,0x805000,2*P,PG_WRITE}}}};
 struct nai75_layout image={.entry=S846_CODE+P+32,.code_pages=2,.data_pages=3,.stack_pages=2,
 .total_pages=7,.code_file=5000,.code_mem=6000,.data_file=4000,.data_mem=9000};
 struct up_context initial={.pc=image.entry,.sp=S846_STACK+2*P,.status=2,.cs=0x5b,.ss=0x63};
 uint32_t pid=99,peer=0;
 struct up_context forged=initial;forged.pc=S846_CODE+2*P;
 REFUSE(rt875_bank_admit_image(&bank,&d,0x1000,0,&child,&lease,&image,"Expanded",&forged,&pid));
 C(pid==99);
 forged=initial;forged.sp++;
 REFUSE(rt875_bank_admit_image(&bank,&d,0x1000,0,&child,&lease,&image,"Expanded",&forged,&pid));
 child.plan.maps[1].flags=PG_WRITE|PG_EXEC;
 REFUSE(rt875_bank_admit_image(&bank,&d,0x1000,0,&child,&lease,&image,"Expanded",&initial,&pid));
 child.plan.maps[1].flags=PG_WRITE;
 C(rt875_bank_admit_image(&bank,&d,0x1000,0,&child,&lease,&image,"Expanded",&initial,&pid)==PS_OK);
 C(rt862_bank_admit(&bank,&d,0x1000,1,102,0x3000,"Legacy",&peer)==PS_OK);
 C(rt862_extent(&bank,0).stack==2&&rt862_extent(&bank,1).stack==1);
 struct rt862_selection a=pick(0,pid);struct up_context c={0};
 C(rt862_bank_enter(&bank,&d,&a,0x1000,&c)&&c.pc==initial.pc&&c.sp==initial.sp);
 struct rt860_tick r=receipt(a,c,0x50),bad=r;
 C(s875_classify_native_frame(&r.frame,va,0,0x2000,0x1000,0x5b,0x63,true,rt862_extent(&bank,0))==RT865_TICK);
 C(rt865_classify_native_frame(&r.frame,va,0,0x2000,0x1000,0x5b,0x63,true)==RT865_INVALID);
 bad.frame.rip=S846_CODE+2*P;REFUSE(rt862_bank_tick(&bank,&d,&a,0x1000,&bad));
 bad=r;bad.frame.rsp=S846_STACK+2*P+1;REFUSE(rt862_bank_tick(&bank,&d,&a,0x1000,&bad));
 r.frame.rip+=8;r.frame.rsp-=16;r.frame.gpr[0]=123;
 C(rt863_bank_event(&bank,&d,&a,0x1000,&r,0)==PS_OK);
 struct rt862_selection z=pick(1,peer);
 C(rt862_bank_enter(&bank,&d,&z,0x1000,&c));
 r=receipt(z,c,0x50);C(rt863_bank_event(&bank,&d,&z,0x1000,&r,0)==PS_OK);
 a=pick(0,pid);C(rt862_bank_enter(&bank,&d,&a,0x1000,&c));
 C(c.pc==initial.pc+8&&c.sp==initial.sp-16&&c.gpr[0]==123);
 r=receipt(a,c,128);r.frame.gpr[0]=S847_CALL_YIELD;
 C(s875_classify_native_frame(&r.frame,va,0,0x2000,0x1000,0x5b,0x63,false,rt862_extent(&bank,0))==RT865_SYSCALL);
 C(rt863_bank_event(&bank,&d,&a,0x1000,&r,0)==PS_OK);
 z=pick(1,peer);C(rt862_bank_enter(&bank,&d,&z,0x1000,&c));
 r=receipt(z,c,0x50);C(rt863_bank_event(&bank,&d,&z,0x1000,&r,0)==PS_OK);
 a=pick(0,pid);C(rt862_bank_enter(&bank,&d,&a,0x1000,&c)&&c.gpr[0]==0);
 r=receipt(a,c,14);r.frame_va+=3*P;r.frame.error=5;
 C(rt863_bank_event(&bank,&d,&a,0x1000,&r,S846_DATA+3*P)==PS_OK);
 C(pr_find(&p,pid)->state==PR_FAULTED&&pr_find(&p,peer)->state==PR_READY);
 C(rt862_bank_release(&bank,&d,0x1000,0,pid,101,0x2000)==PS_OK);
 C(!bank.extents[0].code&&!bank.extents[0].data&&!bank.extents[0].stack);
 z=pick(1,peer);C(rt862_bank_enter(&bank,&d,&z,0x1000,&c));
 r=receipt(z,c,128);r.frame.gpr[0]=S847_CALL_EXIT;
 C(rt863_bank_event(&bank,&d,&z,0x1000,&r,0)==PS_OK);
 C(rt862_bank_release(&bank,&d,0x1000,1,peer,102,0x3000)==PS_OK);
 C(!pr_snapshot(&p,0,0,true));
 uint32_t fresh=0;C(rt862_bank_admit(&bank,&d,0x1000,0,103,0x2000,"Reused",&fresh)==PS_OK&&fresh!=pid);
 C(rt862_extent(&bank,0).code==1&&rt862_extent(&bank,0).stack==1);
 C(rt865_bank_stop_ready(&bank,&d,0x1000,0,fresh,103,0x2000,true)==PS_OK);
 C(rt862_bank_release(&bank,&d,0x1000,0,fresh,103,0x2000)==PS_OK);
 /* Stage 76: stop a real expanded bank entry after a timer receipt, with
  * an independent ready peer. Host receipts are synthetic, not CPL3 proof. */
 uint32_t wide=0,survivor=0,widewin=0,peerwin=0;
 lease.generation=104;child.lease=lease;
 C(rt875_bank_admit_image(&bank,&d,0x1000,0,&child,&lease,&image,"WideStop",&initial,&wide)==PS_OK);
 C(rt862_bank_admit(&bank,&d,0x1000,1,105,0x3000,"Survivor",&survivor)==PS_OK);
 C(ws_open_app(&w,&p,wide,"WideStop",(struct ws_bounds){0,0,400,300},&widewin)==WS_OK);
 C(ws_open_app(&w,&p,survivor,"Survivor",(struct ws_bounds){0,0,400,300},&peerwin)==WS_OK);
 /* Dispatcher next may retain its round-robin cursor from earlier tests. */
 unsigned sid;uint32_t selected;
 C(dpr_dispatch(&d,&sid,&selected)==PS_OK);
 const struct ps_slot *ss=&d.dispatcher.slots[sid];
 struct rt862_selection sel={sid,d.dispatcher.dispatches,selected,ss->handle,ss->user_root,d.dispatcher.safe_root};
 C(rt862_bank_enter(&bank,&d,&sel,0x1000,&c));
 r=receipt(sel,c,0x50);C(rt863_bank_event(&bank,&d,&sel,0x1000,&r,0)==PS_OK);
 if(sid!=0){
  sel=pick(0,wide);C(rt862_bank_enter(&bank,&d,&sel,0x1000,&c));
  r=receipt(sel,c,0x50);C(rt863_bank_event(&bank,&d,&sel,0x1000,&r,0)==PS_OK);
 }
 REFUSE(rt865_bank_stop_ready(&bank,&d,0x1000,0,wide,104,0x2000,false));
 REFUSE(rt865_bank_stop_ready(&bank,&d,0x1000,0,wide,103,0x2000,true));
 const struct rt862_bank_slot keep=bank.slots[1];
 const struct pr_entry keep_entry=*pr_find(&p,survivor);
 C(rt865_bank_stop_ready(&bank,&d,0x1000,0,wide,104,0x2000,true)==PS_OK);
 C(pr_find(&p,wide)->state==PR_EXITED&&pr_find(&p,wide)->cpu_ticks==1);
 C(!ws_find(&w,widewin)&&ws_find(&w,peerwin));
 C(!memcmp(&keep,&bank.slots[1],sizeof keep)&&!memcmp(&keep_entry,pr_find(&p,survivor),sizeof keep_entry));
 C(rt862_bank_release(&bank,&d,0x1000,0,wide,104,0x2000)==PS_OK);
 C(!pr_find(&p,wide)&&!bank.extents[0].code&&!bank.extents[0].data&&!bank.extents[0].stack);
 z=pick(1,survivor);C(rt862_bank_enter(&bank,&d,&z,0x1000,&c));
 r=receipt(z,c,128);r.frame.gpr[0]=S847_CALL_EXIT;
 C(rt863_bank_event(&bank,&d,&z,0x1000,&r,0)==PS_OK);
 C(rt862_bank_release(&bank,&d,0x1000,1,survivor,105,0x3000)==PS_OK);
 C(!pr_snapshot(&p,0,0,true));
 printf("Native expanded context 75: %u checks PASS (synthetic receipts; no hardware execution)\n",checks);
}
