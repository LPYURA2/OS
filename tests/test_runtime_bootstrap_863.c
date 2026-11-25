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
 NO(rt863_bootstrap_ready(0x2000));
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
  NO(rt863_bootstrap(e.root));NO(rt863_bootstrap(e.root));NO(rt863_bootstrap_ready(e.root));NO(rt858_start(&e,&v));NO(rt858_ready(e.root));
  unchanged(&p0,&w0);OK(d==dpr_global_state()&&u==usr_global()&&n==nwc_global());
 }
 printf("S863 shared bootstrap HOST ONLY %s: %u positive / %u refusals PASS\n",mode,good,bad);return 0;
}
