#include <styleos/runtime_entry_858.h>
#include <styleos/desktop_process_runtime.h>
#include <styleos/user_session_runtime.h>
#include <styleos/native_window_channel.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define B(n) (UINT64_C(1)<<(n))
static const struct rt858_expected e={0x1000,0x2000,0x3000,0x4000,0x5390,0x15390,0x57,0x48,0x38};
static const struct rt858_live v={0x2000,0x3000,0x4000,0x14000,B(31)|B(16)|1,B(5),B(10)|B(11),2,0x57,4095,0x48,0x38};
static unsigned good,bad;
#define OK(x) do{assert(x);good++;}while(0)
#define NO(x) do{assert(!(x));bad++;}while(0)
static void unchanged(const struct pr_table *p,const struct ws_service *w){OK(!memcmp(p,pr_global(),sizeof *p)&&!memcmp(w,ws_global(),sizeof *w));}
int main(int argc,char **argv){
 assert(argc==2);const char *mode=argv[1];struct pr_table *p=pr_global();struct ws_service *w=ws_global();
 NO(rt863_bootstrap_ready(0x2000));NO(rt864_services_boundary(0x2000));
 NO(rt864_shell_boundary(e.root,0));NO(rt864_shell_boundary(e.root,&v));NO(rt864_shell_checkpoint(&v));NO(rt864_shell_checkpoint(0));
 if(!strcmp(mode,"ordinary")||!strcmp(mode,"owned")){
  const struct pr_table p0=*p;const struct ws_service w0=*w;
  NO(rt863_bootstrap(0));for(unsigned i=0;i<12;i++)NO(rt863_bootstrap(0x2000|B(i)));for(unsigned i=47;i<64;i++)NO(rt863_bootstrap(0x2000|B(i)));
  unchanged(&p0,&w0);OK(!dpr_global_state()&&!usr_global()&&!nwc_global());
  if(!strcmp(mode,"owned")){OK(rt858_start(&e,&v));OK(rt858_ready(e.root));}
  else {OK(rt863_bootstrap(e.root));NO(rt858_ready(e.root));NO(rt858_start(&e,&v));}
  OK(rt863_bootstrap_ready(e.root));const struct dpr_service *d=dpr_global_state();const struct usr_runtime *u=usr_global();const struct nwc_service *n=nwc_global();
  OK(d&&u&&n&&d->dispatcher.safe_root==e.root&&d->dispatcher.registry==p&&d->windows==w&&u->processes==p&&u->windows==w&&n->registry==p&&n->windows==w&&n->sessions==u);
  struct dpr_service d0=*d;struct usr_runtime u0=*u;struct nwc_service n0=*n;
  for(unsigned i=0;i<64;i++){NO(rt863_bootstrap(e.root^B(i)));NO(rt863_bootstrap_ready(e.root^B(i)));}
  NO(rt863_bootstrap(e.root));NO(rt858_start(&e,&v));OK(rt863_bootstrap_ready(e.root));
  OK(!memcmp(&d0,d,sizeof d0)&&!memcmp(&u0,u,sizeof u0)&&!memcmp(&n0,n,sizeof n0));unchanged(&p0,&w0);
  /* Initial-ready is deliberately not an ongoing runtime-health assertion. */
  p->next_pid=2;NO(rt863_bootstrap_ready(e.root));NO(rt858_ready(e.root));p->next_pid=1;
  w->compositor_generation=2;NO(rt863_bootstrap_ready(e.root));w->compositor_generation=1;
  p->processes[0].pid=7;NO(rt863_bootstrap_ready(e.root));p->processes[0].pid=0;
  w->windows[0].owner_pid=7;NO(rt863_bootstrap_ready(e.root));w->windows[0].owner_pid=0;
  OK(rt863_bootstrap_ready(e.root));
  OK(rt864_services_boundary(e.root));NO(rt864_services_boundary(e.old_root));
  bool owned=!strcmp(mode,"owned");
  OK(rt864_shell_boundary(e.root,owned?&v:0));OK(rt864_shell_checkpoint(&v));
  struct rt858_live checkpoint=v;checkpoint.cr0=0;NO(rt864_shell_checkpoint(&checkpoint));
  checkpoint=v;checkpoint.cr4=0;NO(rt864_shell_checkpoint(&checkpoint));
  checkpoint=v;checkpoint.efer=0;NO(rt864_shell_checkpoint(&checkpoint));
  checkpoint=v;checkpoint.root|=8;if(owned)NO(rt864_shell_checkpoint(&checkpoint));else OK(rt864_shell_checkpoint(&checkpoint));
  checkpoint=v;checkpoint.gdt^=4096;if(owned)NO(rt864_shell_checkpoint(&checkpoint));else OK(rt864_shell_checkpoint(&checkpoint));
  NO(rt864_shell_boundary(e.root,owned?0:&v));
  NO(rt864_shell_boundary(e.old_root,owned?&v:0));
  struct rt858_live wrong=v;wrong.gdt^=4096;NO(rt864_shell_boundary(e.root,&wrong));
  wrong=v;wrong.rflags|=B(9);NO(rt864_shell_boundary(e.root,&wrong));
  wrong=v;wrong.rsp=e.stack_hi;NO(rt864_shell_boundary(e.root,&wrong));
  wrong=v;wrong.root=e.old_root;NO(rt864_shell_boundary(e.root,&wrong));
  struct dpr_service *runtime=(struct dpr_service *)d;
  uint32_t ids[PS_MAX_SLOTS]={0};
  for(unsigned i=0;i<PS_MAX_SLOTS;i++){
   OK(dpr_admit(runtime,i,i+1,0x10000+4096*i,0,"native",&ids[i])==PS_OK);
   OK(rt864_services_boundary(e.root));NO(rt863_bootstrap_ready(e.root));
   if(owned){OK(rt864_shell_boundary(e.root,&v));OK(rt864_shell_checkpoint(&v));}else{NO(rt864_shell_boundary(e.root,0));NO(rt864_shell_checkpoint(&v));}
  }
  struct ps_dispatcher *ps=&runtime->dispatcher;
  struct ps_slot old=ps->slots[1];ps->slots[1].handle=ps->slots[0].handle;
  NO(rt864_services_boundary(e.root));ps->slots[1]=old;
  ps->slots[1].user_root=e.root;NO(rt864_services_boundary(e.root));ps->slots[1]=old;
  ps->slots[1].pid=0;NO(rt864_services_boundary(e.root));ps->slots[1]=old;
  for(unsigned i=0;i<PS_MAX_SLOTS;i++){
   unsigned chosen=99;uint32_t pid=0;
   OK(dpr_dispatch(runtime,&chosen,&pid)==PS_OK&&chosen==i&&pid==ids[i]);
   NO(rt864_services_boundary(e.root));
   NO(rt864_shell_boundary(e.root,owned?&v:0));
   OK(dpr_observe(runtime,i,i+1,0x10000+4096*i,e.root,PS_EXIT)==PS_OK);
   OK(rt864_services_boundary(e.root));
   OK(dpr_release(runtime,i,i+1)==PS_OK);
   OK(rt864_services_boundary(e.root));
  }
  NO(rt863_bootstrap_ready(e.root));
  OK(!pr_snapshot(p,0,0,true)&&ps->dispatches==PS_MAX_SLOTS);
  if(owned)OK(rt864_shell_boundary(e.root,&v));else NO(rt864_shell_boundary(e.root,0));
  if(owned){
   uint32_t sid,pid,win;struct usr_runtime *users=(struct usr_runtime *)u;struct nwc_service *channel=(struct nwc_service *)n;
   OK(usr_create(users,1,&sid)==USR_OK&&usr_select(users,sid)==USR_OK);
   OK(dpr_admit(runtime,0,999,0x90000,0,"ipc",&pid)==PS_OK&&usr_bind(users,sid,pid,999)==USR_OK);
   OK(ws_open_app(w,p,pid,"ipc",(struct ws_bounds){0,0,400,300},&win)==WS_OK&&nwc_claim(channel,pid,999,0x90000,win)==NWC_OK);
   /* A parked context retains its existing port, window and session.
    * This tests only the service graph, not hardware context admission. */
   OK(pr_transition(p,pid,999,PR_WAITING)==PR_OK&&rt864_shell_boundary(e.root,&v));
   OK(nwc_send_key(channel,win,65,0,true)==NWC_OK&&rt864_shell_boundary(e.root,&v));
   uint64_t handle_saved=channel->ports[0].scheduler_handle;
   channel->ports[0].scheduler_handle^=1;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].scheduler_handle=handle_saved;
   unsigned waiting_slot=99;uint32_t waiting_pid=0;
   OK(dpr_dispatch(runtime,&waiting_slot,&waiting_pid)==PS_EMPTY);
   OK(pr_transition(p,pid,999,PR_READY)==PR_OK&&rt864_shell_boundary(e.root,&v));
   OK(nwc_send_pointer(channel,win,-1,7,1,0)==NWC_OK&&rt864_shell_boundary(e.root,&v));
   struct nwc_event pointer=channel->ports[0].events[1];
   channel->ports[0].events[1].size=7;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].events[1]=pointer;
   channel->ports[0].events[1].code=8;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].events[1]=pointer;
   channel->ports[0].events[1].modifiers=16;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].events[1]=pointer;
   channel->ports[0].events[1].down=false;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].events[1]=pointer;
   channel->ports[0].events[1].bytes[8]=1;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].events[1]=pointer;
   channel->ports[0].events[1].kind=99;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].events[1]=pointer;
   OK(rt864_shell_boundary(e.root,&v));
   const struct nwc_rect frame[]={{0,0,400,300,0x123456},{3,4,9,10,0xabcdef}};
   OK(nwc_publish_frame(channel,pid,999,0x90000,win,frame,2)==NWC_OK&&rt864_shell_boundary(e.root,&v));
   const struct nwc_frame frame_saved=channel->ports[0].frame;
   channel->ports[0].frame.count=NWC_FRAME_RECTS+1;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].frame=frame_saved;
   channel->ports[0].frame.rect[0].w=399;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].frame=frame_saved;
   channel->ports[0].frame.rect[1].x=UINT32_MAX;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].frame=frame_saved;
   channel->ports[0].frame.rect[1].rgb=0x1000000;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].frame=frame_saved;
   channel->ports[0].frame.rect[2].w=1;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].frame=frame_saved;
   channel->ports[0].frame.generation=0;NO(rt864_shell_boundary(e.root,&v));channel->ports[0].frame=frame_saved;
   OK(rt864_shell_boundary(e.root,&v));
   struct nwc_port saved=channel->ports[0];channel->ports[0].user_root^=4096;NO(rt864_shell_boundary(e.root,&v));channel->ports[0]=saved;
   channel->ports[0].head=NWC_EVENTS;NO(rt864_shell_boundary(e.root,&v));channel->ports[0]=saved;
   channel->ports[0].events[0].sequence=8;NO(rt864_shell_boundary(e.root,&v));channel->ports[0]=saved;
   channel->ports[1]=saved;NO(rt864_shell_boundary(e.root,&v));memset(&channel->ports[1],0,sizeof channel->ports[1]);
   unsigned slot;uint32_t actual;OK(dpr_dispatch(runtime,&slot,&actual)==PS_OK&&actual==pid&&slot==0);
   OK(dpr_observe(runtime,0,999,0x90000,e.root,PS_EXIT)==PS_OK);NO(rt864_shell_boundary(e.root,&v));
   OK(usr_reconcile(users)==1&&nwc_reconcile(channel)==1&&rt864_shell_boundary(e.root,&v));
   OK(dpr_release(runtime,0,999)==PS_OK&&usr_lock(users)==USR_OK&&usr_close(users,sid)==USR_OK&&rt864_shell_boundary(e.root,&v));
  }
 }else{
  if(!strcmp(mode,"dpr"))OK(dpr_global_start(0x1000)==DPR_OK);
  else if(!strcmp(mode,"usr"))OK(usr_global_start()==USR_OK);
  else if(!strcmp(mode,"ipc")){OK(usr_global_start()==USR_OK);OK(nwc_global_start()==NWC_OK);}
  else if(!strcmp(mode,"pid-counter"))p->next_pid=2;
  else if(!strcmp(mode,"serial"))p->change_serial=1;
  else if(!strcmp(mode,"window-counter"))w->next_id=2;
  else if(!strcmp(mode,"compositor"))w->compositor_generation=2;
  else if(!strcmp(mode,"unused-row"))p->processes[31].scheduler_handle=7;
  else if(!strcmp(mode,"unused-window"))w->windows[23].owner_pid=8;
  else if(!strcmp(mode,"dead-process")){uint32_t pid;struct pr_admission a={.type=PR_NATIVE_USER,.scheduler_handle=7,.page_root=0x4000,.label="dead"};OK(pr_admit(p,&a,&pid)==PR_OK);OK(pr_transition(p,pid,7,PR_RUNNING)==PR_OK);OK(pr_transition(p,pid,7,PR_EXITED)==PR_OK);}
  else assert(0);
  const struct pr_table p0=*p;const struct ws_service w0=*w;const struct dpr_service *d=dpr_global_state();const struct usr_runtime *u=usr_global();const struct nwc_service *n=nwc_global();
  NO(rt863_bootstrap(e.root));NO(rt863_bootstrap(e.root));NO(rt863_bootstrap_ready(e.root));NO(rt858_start(&e,&v));NO(rt858_ready(e.root));NO(rt864_services_boundary(e.root));
  unchanged(&p0,&w0);OK(d==dpr_global_state()&&u==usr_global()&&n==nwc_global());
 }
 printf("S863 shared bootstrap HOST ONLY %s: %u positive / %u refusals PASS\n",mode,good,bad);return 0;
}
