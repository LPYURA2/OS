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
 printf("S865 forced stop HOST ONLY: %u positive, %u refusals PASS\n",positive,negative);
 return 0;
}
