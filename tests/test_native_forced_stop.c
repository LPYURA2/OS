#include <styleos/runtime_bank_862.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank b,b0;
static struct dpr_service d,d0;
static struct pr_table p,p0;
static struct ws_service w,w0;
static unsigned positive,negative;
#define OK(x) do{assert(x);++positive;}while(0)
#define REFUSE(expr,status) do{b0=b;d0=d;p0=p;w0=w;assert((expr)==(status)); \
 assert(!memcmp(&b,&b0,sizeof b)&&!memcmp(&d,&d0,sizeof d)&&!memcmp(&p,&p0,sizeof p)&&!memcmp(&w,&w0,sizeof w));++negative;}while(0)
#define STOP(root,slot,pid,handle,child,auth) rt865_bank_stop_ready(&b,&d,root,slot,pid,handle,child,auth)
int main(void){
 uint32_t a=0,z=0,wa=0,wz=0;
 pr_init(&p);ws_init(&w);
 OK(dpr_start(&d,&p,&w,0x1000)==DPR_OK);
 OK(rt862_bank_open(&b,&d,0x1000,0x5b,0x63));
 OK(rt862_bank_admit(&b,&d,0x1000,0,101,0x2000,"Hang",&a)==PS_OK);
 OK(rt862_bank_admit(&b,&d,0x1000,1,102,0x3000,"Peer",&z)==PS_OK);
 OK(ws_open_app(&w,&p,a,"Hang",(struct ws_bounds){0,0,400,300},&wa)==WS_OK);
 OK(ws_open_app(&w,&p,z,"Peer",(struct ws_bounds){0,0,400,300},&wz)==WS_OK);
 REFUSE(STOP(0x1000,0,a,101,0x2000,false),PS_EVIDENCE);
 REFUSE(STOP(0x3000,0,a,101,0x2000,true),PS_EVIDENCE);
 REFUSE(STOP(0x1000,PS_MAX_SLOTS,a,101,0x2000,true),PS_EVIDENCE);
 REFUSE(STOP(0x1000,0,z,101,0x2000,true),PS_EVIDENCE);
 REFUSE(STOP(0x1000,1,a,101,0x2000,true),PS_EVIDENCE);
 REFUSE(STOP(0x1000,0,a,102,0x2000,true),PS_EVIDENCE);
 REFUSE(STOP(0x1000,0,a,101,0x3000,true),PS_EVIDENCE);
 unsigned slot;uint32_t selected;
 OK(dpr_dispatch(&d,&slot,&selected)==PS_OK&&slot==0&&selected==a);
 REFUSE(STOP(0x1000,0,a,101,0x2000,true),PS_STATE);
 REFUSE(STOP(0x1000,1,z,102,0x3000,true),PS_STATE);
 struct rt862_selection t={0,d.dispatcher.dispatches,a,101,0x2000,0x1000};
 struct up_context c={0};OK(rt862_bank_enter(&b,&d,&t,0x1000,&c));
 struct rt860_tick tick={.slot=0,.dispatch=t.dispatch,.pid=a,.handle=101,.child_root=0x2000,.safe_root=0x1000,
 .frame_va=S846_IST+4096-16-sizeof(struct s847_raw),.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,
 .frame={.vector=0x50,.rip=c.pc,.rsp=c.sp,.cs=c.cs,.ss=c.ss,.rflags=c.status}};
 OK(rt862_bank_tick(&b,&d,&t,0x1000,&tick)==PS_OK);
 uint32_t serial=p.change_serial;p.change_serial=UINT32_MAX-1;
 REFUSE(STOP(0x1000,0,a,101,0x2000,true),PS_EXHAUSTED);p.change_serial=serial;
 const struct rt862_bank_slot peer=b.slots[1];const struct pr_entry peer_p=*pr_find(&p,z);
 unsigned dispatches=d.dispatcher.dispatches,next=d.dispatcher.next;
 OK(STOP(0x1000,0,a,101,0x2000,true)==PS_OK);
 OK(pr_find(&p,a)->state==PR_EXITED&&pr_find(&p,a)->cpu_ticks==1);
 OK(!ws_find(&w,wa)&&ws_find(&w,wz));
 OK(!memcmp(&peer,&b.slots[1],sizeof peer)&&!memcmp(&peer_p,pr_find(&p,z),sizeof peer_p));
 OK(d.dispatcher.current==-1&&d.dispatcher.next==next&&d.dispatcher.dispatches==dispatches);
 struct up_context zero={0};OK(!memcmp(&zero,&b.slots[0].context,sizeof zero));
 REFUSE(STOP(0x1000,0,a,101,0x2000,true),PS_STATE);
 /* Host model: real FP/root/lease cleanup remains a separate guest gate. */
 OK(rt862_bank_release(&b,&d,0x1000,0,a,101,0x2000)==PS_OK);
 OK(!pr_find(&p,a)&&!b.slots[0].pid&&p.change_serial==serial+2);
 uint32_t fresh=0;OK(rt862_bank_admit(&b,&d,0x1000,0,103,0x2000,"Restart",&fresh)==PS_OK&&fresh!=a);
 REFUSE(STOP(0x1000,0,a,101,0x2000,true),PS_EVIDENCE);
 REFUSE(STOP(0x1000,0,fresh,101,0x2000,true),PS_EVIDENCE);
 OK(dpr_dispatch(&d,&slot,&selected)==PS_OK&&slot==1&&selected==z);
 /* Independent wait-state contract on a fresh bank, with live peer. */
 static struct rt862_bank wb;
 static struct dpr_service wd;
 static struct pr_table wp;
 static struct ws_service ww;
 pr_init(&wp);ws_init(&ww);
 OK(dpr_start(&wd,&wp,&ww,0x1000)==DPR_OK);
 OK(rt862_bank_open(&wb,&wd,0x1000,0x5b,0x63));
 uint32_t sleeper=0,runner=0,sw=0,rw=0;
 OK(rt862_bank_admit(&wb,&wd,0x1000,0,201,0x4000,"Waiter",&sleeper)==PS_OK);
 OK(rt862_bank_admit(&wb,&wd,0x1000,1,202,0x5000,"Runner",&runner)==PS_OK);
 OK(ws_open_app(&ww,&wp,sleeper,"Waiter",(struct ws_bounds){0,0,400,300},&sw)==WS_OK);
 OK(ws_open_app(&ww,&wp,runner,"Runner",(struct ws_bounds){0,0,400,300},&rw)==WS_OK);
 struct up_context saved=wb.slots[0].context;
 #define WAIT(pid,gen,root,waiting) rt865_bank_wait_state(&wb,&wd,0x1000,0,pid,gen,root,waiting)
 #define WREFUSE(expr,status) do{b0=wb;d0=wd;p0=wp;w0=ww;assert((expr)==(status)); \
 assert(!memcmp(&wb,&b0,sizeof wb)&&!memcmp(&wd,&d0,sizeof wd)&&!memcmp(&wp,&p0,sizeof wp)&&!memcmp(&ww,&w0,sizeof ww));++negative;}while(0)
 WREFUSE(WAIT(sleeper,201,0x4000,false),PS_STATE);
 WREFUSE(WAIT(sleeper,202,0x4000,true),PS_EVIDENCE);
 WREFUSE(WAIT(runner,201,0x4000,true),PS_EVIDENCE);
 WREFUSE(WAIT(sleeper,201,0x5000,true),PS_EVIDENCE);
 OK(WAIT(sleeper,201,0x4000,true)==PS_OK);
 OK(pr_find(&wp,sleeper)->state==PR_WAITING&&!memcmp(&saved,&wb.slots[0].context,sizeof saved));
 WREFUSE(WAIT(sleeper,201,0x4000,true),PS_STATE);
 OK(dpr_dispatch(&wd,&slot,&selected)==PS_OK&&slot==1&&selected==runner);
 WREFUSE(WAIT(sleeper,201,0x4000,false),PS_STATE);
 struct rt862_selection wt={1,wd.dispatcher.dispatches,runner,202,0x5000,0x1000};
 OK(rt862_bank_enter(&wb,&wd,&wt,0x1000,&c));
 tick=(struct rt860_tick){.slot=1,.dispatch=wt.dispatch,.pid=runner,.handle=202,.child_root=0x5000,.safe_root=0x1000,
 .frame_va=S846_IST+4096-16-sizeof(struct s847_raw),.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,
 .frame={.vector=0x50,.rip=c.pc,.rsp=c.sp,.cs=c.cs,.ss=c.ss,.rflags=c.status}};
 OK(rt862_bank_tick(&wb,&wd,&wt,0x1000,&tick)==PS_OK);
 OK(WAIT(sleeper,201,0x4000,false)==PS_OK&&pr_find(&wp,sleeper)->state==PR_READY);
 OK(!memcmp(&saved,&wb.slots[0].context,sizeof saved));
 OK(WAIT(sleeper,201,0x4000,true)==PS_OK);
 WREFUSE(rt865_bank_stop_ready(&wb,&wd,0x1000,0,sleeper,201,0x4000,false),PS_EVIDENCE);
 OK(rt865_bank_stop_ready(&wb,&wd,0x1000,0,sleeper,201,0x4000,true)==PS_OK);
 OK(!ws_find(&ww,sw)&&ws_find(&ww,rw)&&pr_find(&wp,runner)->state==PR_READY);
 OK(rt862_bank_release(&wb,&wd,0x1000,0,sleeper,201,0x4000)==PS_OK);
 WREFUSE(WAIT(sleeper,201,0x4000,false),PS_EVIDENCE);
 #undef WAIT
 #undef WREFUSE
 printf("S865 forced stop HOST ONLY: %u positive, %u refusals PASS\n",positive,negative);
 return 0;
}
