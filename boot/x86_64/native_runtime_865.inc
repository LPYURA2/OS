/* Ordinary native runtime: reuse the same audited detached-root and
 * hardware execution path without the historical batch driver. Included
 * only by the owned-root architecture after all root helpers are defined. */
#include "native_executor_865.h"
#include <styleos/native_runtime_865.h>
static struct s846_root native_roots[S846_ROOT_COUNT];
static struct s844_lease native_leases[S846_ROOT_COUNT];
static struct s844_arena *native_arena;
static const struct boot_memory_evidence *native_evidence;
static struct s846_guard native_guard;
static struct s842_cache native_cache;
static bool native_ready;
static uint8_t native_scratch[8192];
static void native_wipe(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static bool native_audit(void *unused){
 (void)unused;
 return native_arena&&native_evidence&&read_cr3()==composed.built.root&&!(flags()&0x200)&&same_cache(&native_cache)&&
  rt865_roots_audit_sparse_frozen(base861,&native_cache,&frozen861,native_evidence,native_arena,native_leases,&native_guard,&composed,native_roots);
}
volatile uint8_t *s865_native_data_span(unsigned id,uint32_t pid,uint64_t gen,uint64_t root,
                                        uint64_t va,size_t bytes){
 if(!native_ready||!bytes||bytes>4096u||va<S846_DATA||
    va-S846_DATA>4096u-bytes||id>=S846_ROOT_COUNT||!runtime860||
    runtime860->dispatcher.current!=-1||!native_audit(0))return 0;
 const struct pr_entry *caller=pr_find(pr_global(),pid);
 const struct s844_lease lease=native_leases[id];
 if(!caller||caller->state!=PR_READY||caller->scheduler_handle!=gen||caller->page_root!=root||
    bank862.slots[id].pid!=pid||bank862.slots[id].handle!=gen||bank862.slots[id].root!=root||
    !native_roots[id].ready||native_roots[id].built.root!=root||
    lease.generation!=gen||lease.pages!=3||lease.owner!=S846_OWNER_A+id||
    !s844_lease_live(native_arena,lease))return 0;
 /* native_audit verifies the real detached PTEs and identity mapped
  * safe-root arena, with this exact live generation and three-page lease.
  * The data page is the second page; the user VA itself is never accessed. */
 return (volatile uint8_t *)(uintptr_t)(lease.pa+4096u+(va-S846_DATA));
}
bool s865_native_copy_data(unsigned id,uint32_t pid,uint64_t gen,uint64_t root,
                           uint64_t va,void *out,size_t bytes){
 if(!out)return false;
 const volatile uint8_t *src=s865_native_data_span(id,pid,gen,root,va,bytes);
 if(!src)return false;
 uint8_t *dst=out;
 for(size_t i=0;i<bytes;++i)dst[i]=src[i];
 return true;
}
static bool native_release(void *unused,unsigned id,uint64_t root,uint64_t generation){
 (void)unused;
 if(id>=S846_ROOT_COUNT||!native_audit(0)||native_roots[id].built.root!=root||
    native_leases[id].generation!=generation||!s844_lease_live(native_arena,native_leases[id]))return false;
 struct s844_lease lease=native_leases[id];
 native_wipe(&native_roots[id],sizeof native_roots[id]);
 native_wipe(child_tables861[id],sizeof child_tables861[id]);
 if(!s844_release(native_arena,lease.owner,lease))return false;
 native_wipe(&native_leases[id],sizeof native_leases[id]);
 return native_audit(0);
}
static bool native_boot(struct pg_experiment *p,const struct s842_cache *cache,const struct s846_guard *guard,struct s844_arena *arena){
 if(native_ready||!p||!p->evidence||!cache||!guard||!arena||!base861)return false;
 native_arena=arena;native_evidence=p->evidence;native_guard=*guard;native_cache=*cache;
 if(!native_audit(0)||!s865_executor_boot(composed.built.root,base861->lapic,native_audit,0))return false;
 native_ready=true;
 arch_puts("STYLEOS NATIVE RUNTIME READY");field("slots",S846_ROOT_COUNT);field("pages",S844_PAGES);field("root",read_cr3());arch_putc('\n');
 return true;
}
enum ps_status s865_native_launch(const struct rt865_launch_request *request,uint32_t *pid){
 if(!request||!pid)return PS_ARGUMENT;
 if(!native_ready||!s865_executor_idle()||!native_audit(0))return PS_STATE;
 if(pr_global()->change_serial>UINT32_MAX-128u)return PS_EXHAUSTED;
 unsigned id=0;
 while(id<S846_ROOT_COUNT&&native_leases[id].generation)++id;
 if(id==S846_ROOT_COUNT)return PS_FULL;
 if(!s844_acquire(native_arena,S846_OWNER_A+id,3,&native_leases[id]))return PS_FULL;
 if(!s865_root_build(base861,&native_cache,native_arena,native_leases,id,&native_guard,child_tables861[id],
    (uintptr_t)child_tables861[id],&native_roots[id])){
  native_wipe(&native_roots[id],sizeof native_roots[id]);
  native_wipe(child_tables861[id],sizeof child_tables861[id]);
  if(!s844_release(native_arena,native_leases[id].owner,native_leases[id]))fail("native-build-rollback");
  native_wipe(&native_leases[id],sizeof native_leases[id]);
  if(!native_audit(0))fail("native-build-audit");
  return PS_EVIDENCE;
 }
 struct nte_loaded image={0};uint32_t assigned=0;
 enum ps_status result=s865_executor_launch(id,&native_roots[id],native_arena,&native_leases[id],request,
    native_scratch,sizeof native_scratch,native_audit,0,&image,&assigned);
 if(result!=PS_OK){
  if(!native_release(0,id,native_roots[id].built.root,native_leases[id].generation))fail("native-launch-rollback");
  return result;
 }
 if(pr_account(pr_global(),assigned,native_leases[id].generation,0,3*4096)!=PR_OK||
    !s865_executor_idle()||!native_audit(0))fail("native-launch-publish");
 *pid=assigned;
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
 arch_puts("STYLEOS NATIVE ADMIT");field("slot",id);field("pid",assigned);field("gen",native_leases[id].generation);field("root",native_roots[id].built.root);field("entry",image.entry);arch_putc('\n');
 arch_puts("STYLEOS NATIVE LEASE");field("pid",assigned);field("gen",native_leases[id].generation);field("pa",native_leases[id].pa);field("pages",native_leases[id].pages);arch_putc('\n');
#endif
 return PS_OK;
}
enum ps_status s865_native_stop(uint32_t pid,uint64_t generation,bool authorized){
 if(!authorized||!pid||!generation)return PS_EVIDENCE;
 if(!native_ready||!s865_executor_idle()||runtime860->dispatcher.current!=-1)
  return PS_STATE;
 if(!native_audit(0))return PS_EVIDENCE;
 unsigned id=0;
 while(id<S846_ROOT_COUNT&&bank862.slots[id].pid!=pid)++id;
 if(id==S846_ROOT_COUNT||bank862.slots[id].handle!=generation||
    native_leases[id].generation!=generation||
    !s844_lease_live(native_arena,native_leases[id])||
    bank862.slots[id].root!=native_roots[id].built.root)return PS_EVIDENCE;
 return s865_executor_stop(id,pid,generation,native_roots[id].built.root,
                           authorized,native_audit,native_release,0);
}
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
static unsigned window_launch_fault;
#define WINDOW_LAUNCH_FAULT(step) (window_launch_fault==(step))
#else
#define WINDOW_LAUNCH_FAULT(step) false
#endif
enum ps_status s865_native_launch_window(const struct rt865_launch_request *request,
 uint32_t session,const char *title,struct ws_bounds bounds,struct s865_window_launch *out){
 if(!request||!out||!title||!session)return PS_ARGUMENT;
 if(!native_ready||!s865_executor_idle()||!native_audit(0))return PS_STATE;
 struct usr_runtime *u=(struct usr_runtime *)usr_global();
 struct nwc_service *n=(struct nwc_service *)nwc_global();
 struct ws_service *w=ws_global();
 struct pr_table *registry=pr_global();
 if(!u||!n||!u->ready||!n->ready||u->processes!=registry||u->windows!=w||
    n->registry!=registry||n->windows!=w||n->sessions!=u)return PS_STATE;
 const struct usr_session *s=usr_find(u,session);
 if(!s||s->state!=USR_ACTIVE||u->selected_session!=session)return PS_EVIDENCE;
 if(bounds.width<=0||bounds.height<=0||bounds.width>8192||bounds.height>8192||
    bounds.x < -8192||bounds.x>8192||bounds.y < -8192||bounds.y>8192)return PS_ARGUMENT;
 unsigned len=0;
 while(len<WS_TITLE_BYTES&&title[len]){
  unsigned char c=(unsigned char)title[len++];
  if(c<0x20||c==0x7f)return PS_ARGUMENT;
 }
 if(!len||len==WS_TITLE_BYTES)return PS_ARGUMENT;
 if(!w->next_id||w->next_id==UINT32_MAX)return PS_EXHAUSTED;
 if(nwc_port_count(n)>=NWC_PORTS||ws_snapshot(w,0,0)>=WS_MAX_WINDOWS)return PS_FULL;
 uint32_t pid=0,win=0;
 enum ps_status status=s865_native_launch(request,&pid);
 if(status!=PS_OK)return status;
 const struct pr_entry *entry=pr_find(registry,pid);
 if(!entry||entry->state!=PR_READY||!entry->scheduler_handle)fail("native-window-launch-owner");
 const uint64_t gen=entry->scheduler_handle,root=entry->page_root;
 if(WINDOW_LAUNCH_FAULT(1)||usr_bind(u,session,pid,gen)!=USR_OK||
    WINDOW_LAUNCH_FAULT(2)||ws_open_app(w,registry,pid,title,bounds,&win)!=WS_OK||
    WINDOW_LAUNCH_FAULT(3)||nwc_claim(n,pid,gen,root,win)!=NWC_OK||
    WINDOW_LAUNCH_FAULT(4)){
  if(s865_native_stop(pid,gen,true)!=PS_OK||pr_find(registry,pid)||
     (win&&ws_find(w,win))||!native_audit(0))fail("native-window-launch-rollback");
  return PS_EVIDENCE;
 }
 if(!s865_executor_idle()||!native_audit(0))fail("native-window-launch-publish");
 *out=(struct s865_window_launch){pid,win};
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
 arch_puts("STYLEOS NATIVE WINDOW-LAUNCH");field("pid",pid);field("gen",gen);
 field("root",root);field("session",session);field("window",win);arch_putc('\n');
#endif
 return PS_OK;
}
#undef WINDOW_LAUNCH_FAULT
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
bool s865_native_probe_window_rollback(const struct rt865_launch_request *request,uint32_t session){
 unsigned used=0;
 const struct usr_runtime *u=usr_global();
 const struct nwc_service *n=nwc_global();
 struct ws_service *w=ws_global();
 if(!request||!u||!n||!native_audit(0)||!s865_executor_idle()||window_launch_fault||
    pr_snapshot(pr_global(),0,0,true)||ws_snapshot(w,0,0)||nwc_port_count(n)||
    !s844_count(native_arena,&used)||used)return false;
 uint64_t dispatches=runtime860->dispatcher.dispatches;
 arch_puts("STYLEOS NATIVE ROLLBACK-BEGIN");field("dispatches",dispatches);arch_putc('\n');
 for(unsigned phase=1;phase<=4;++phase){
  struct s865_window_launch out={0xabcdef,0x123456};
  uint64_t next=native_arena->next;
  uint32_t next_win=w->next_id;
  window_launch_fault=phase;
  enum ps_status status=s865_native_launch_window(request,session,"Rollback probe",(struct ws_bounds){0,0,160,120},&out);
  window_launch_fault=0;
  if(status!=PS_EVIDENCE||out.pid!=0xabcdef||out.window!=0x123456||
     !s865_executor_idle()||!native_audit(0)||!s844_count(native_arena,&used)||used||
     native_arena->next!=next+1||w->next_id!=next_win+(phase>=3?1u:0u)||
     runtime860->dispatcher.dispatches!=dispatches||pr_snapshot(pr_global(),0,0,true)||
     ws_snapshot(w,0,0)||nwc_port_count(n))return false;
  for(unsigned i=0;i<PR_MAX_PROCESSES;++i)
   if(u->owners[i].pid||u->owners[i].session_id||u->owners[i].scheduler_handle)return false;
  const volatile uint8_t *pages=(const volatile uint8_t *)(uintptr_t)native_arena->base;
  for(size_t i=0;i<3u*4096u;++i)if(pages[i])return false;
  arch_puts("STYLEOS NATIVE ROLLBACK-PHASE");field("phase",phase);
  field("used",used);field("resident",pr_snapshot(pr_global(),0,0,true));
  field("windows",ws_snapshot(w,0,0));field("ports",nwc_port_count(n));
  field("dispatches",dispatches);arch_putc('\n');
 }
 arch_puts("STYLEOS NATIVE ROLLBACK-END");field("dispatches",dispatches);arch_putc('\n');
 return true;
}
#endif
enum ps_status s865_native_stop_selected(uint32_t pid){
 if(!native_ready||!s865_executor_idle()||runtime860->dispatcher.current!=-1)return PS_STATE;
 if(!native_audit(0))return PS_EVIDENCE;
 const struct pr_table *p=runtime860->dispatcher.registry;
 if(p!=pr_global()||pi_can_request_end(p,pid)!=PI_TARGET_ELIGIBLE)return PS_EVIDENCE;
 const struct pr_entry *e=pr_find(p,pid);
 if(!e||e->state!=PR_READY)return PS_STATE;
 return s865_native_stop(pid,e->scheduler_handle,true);
}
enum pa_status s865_native_snapshot(const struct pa_request *q,struct pa_row *rows,
                                    size_t capacity,struct pa_reply *reply){
 if(!native_ready||!s865_executor_idle()||!native_audit(0)||
    runtime860->dispatcher.current!=-1)return PA_DENIED;
 /* Verify provenance of every live row before publishing measurement validity.
  * No fake system tasks or CPU percentages. Reject unaccounted registry rows. */
 const struct pr_table *p=runtime860->dispatcher.registry;
 if(!p||p!=pr_global())return PA_DENIED;
 for(unsigned i=0;i<PR_MAX_PROCESSES;i++){
  const struct pr_entry *e=&p->processes[i];
  if(e->state!=PR_READY&&e->state!=PR_RUNNING&&e->state!=PR_WAITING)continue;
  if(e->type!=PR_NATIVE_USER||e->state==PR_RUNNING||e->resident_bytes!=3*4096u)return PA_DENIED;
  unsigned id=0;
  while(id<S846_ROOT_COUNT&&bank862.slots[id].pid!=e->pid)++id;
  if(id==S846_ROOT_COUNT||bank862.slots[id].handle!=e->scheduler_handle||
     native_leases[id].generation!=e->scheduler_handle||native_leases[id].pages!=3||
     native_roots[id].built.root!=e->page_root||!s844_lease_live(native_arena,native_leases[id]))return PA_DENIED;
 }
 enum pa_status r=dpr_supervisor_snapshot(runtime860,q,rows,capacity,reply);
 if(r==PA_OK||r==PA_TRUNCATED)reply->measurement_flags=PA_MEASURE_CPU_TICKS|PA_MEASURE_MEMORY_BYTES;
 return r;
}
static struct { uint32_t pid;uint64_t gen,root,epoch; } shell_controller;
static uint64_t shell_epoch,shell_redraw,shell_refresh_commit;
static bool shell_refresh_pending;
static unsigned shell_pending_key,shell_commit_action;
enum { SHELL_ATTEMPTS=3, SHELL_SLICE_LIMIT=32 };
static struct {
 struct rt865_launch_request request;
 bool configured,exhausted;
 unsigned attempts,delay,slices;
 uint32_t pid;
 uint64_t gen;
} shell_recovery;
static void shell_controller_reconcile(void){
 if(shell_controller.pid){
  const struct pr_entry *e=pr_find(pr_global(),shell_controller.pid);
  if(!e||e->scheduler_handle!=shell_controller.gen||e->page_root!=shell_controller.root||
     !pr_is_live(pr_global(),shell_controller.pid))
   native_wipe(&shell_controller,sizeof shell_controller);
 }
}
enum ps_status s865_shell_controller_launch(const struct rt865_launch_request *request,uint32_t *out){
 if(!request||!out)return PS_ARGUMENT;
 if(!native_ready||!s865_executor_idle()||!native_audit(0))return PS_STATE;
 shell_controller_reconcile();
 if(shell_controller.pid)return PS_STATE;
 if(shell_epoch==UINT32_MAX)return PS_EXHAUSTED;
 uint32_t pid=0;
 enum ps_status status=s865_native_launch(request,&pid);
 if(status!=PS_OK)return status;
 const struct pr_entry *e=pr_find(pr_global(),pid);
 if(!e||e->state!=PR_READY||!e->scheduler_handle||!e->page_root)fail("shell-controller-admit");
 shell_controller.pid=pid;shell_controller.gen=e->scheduler_handle;
 shell_controller.root=e->page_root;shell_controller.epoch=++shell_epoch;
 *out=pid;
 arch_puts("STYLEOS NATIVE SHELL-CONTROLLER");field("pid",pid);field("gen",e->scheduler_handle);
 field("root",e->page_root);field("epoch",shell_epoch);arch_putc('\n');
 return PS_OK;
}
static bool shell_controller_auth(uint32_t pid,uint64_t gen,uint64_t root){
 if(!pid||!gen||!root||pid!=shell_controller.pid||gen!=shell_controller.gen||
    root!=shell_controller.root||!shell_controller.epoch||!native_audit(0))return false;
 const struct pr_entry *e=pr_find(pr_global(),pid);
 return e&&e->state==PR_READY&&e->scheduler_handle==gen&&e->page_root==root;
}
uint64_t s865_shell_controller_request(uint32_t pid,uint64_t gen,uint64_t root){
 if(!shell_controller_auth(pid,gen,root))return 0;
 shell_redraw=shell_controller.epoch;return shell_redraw;
}
enum ps_status s865_shell_configure_recovery(const struct rt865_launch_request *request){
 if(!request||!request->source.read)return PS_ARGUMENT;
 if(!native_ready||!s865_executor_idle()||!native_audit(0))return PS_STATE;
 if(shell_recovery.configured||shell_controller.pid||shell_refresh_commit)return PS_STATE;
 shell_recovery.request=*request;shell_recovery.configured=true;
 return PS_OK;
}
bool s865_shell_recovery_busy(void){return shell_recovery.configured&&!shell_recovery.exhausted&&(shell_refresh_pending||shell_recovery.pid);}
bool s865_shell_recovery_exhausted(void){return shell_recovery.exhausted;}
static bool shell_submit_key(unsigned key){
 if(!native_ready||!s865_executor_idle()||!native_audit(0))return false;
 shell_controller_reconcile();
 if(shell_recovery.exhausted)return true;
 if(!shell_controller.pid&&!shell_recovery.configured){
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
  if(!s865_native_shell_probe_available())return false;
#else
  return false;
#endif
 }
 if(!shell_refresh_pending&&!shell_refresh_commit){
  shell_recovery.attempts=0;shell_recovery.delay=0;
  shell_refresh_pending=true;shell_pending_key=key;
  arch_puts("STYLEOS NATIVE SHELL-SUBMIT");field("key",key);arch_putc('\n');
 }
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
 if(!shell_controller.pid&&!shell_recovery.configured)s865_native_request_shell_gap();
#endif
 return true;
}
bool s865_shell_submit_refresh(void){return shell_submit_key(0x44);}
bool s865_shell_submit_command(unsigned key){
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
 (void)key;return false;
#else
 if(key!=0x3e&&key!=0x42)return false;
 return shell_submit_key(key);
#endif
}
bool s865_shell_command_waiting(void){
 return !shell_recovery.exhausted&&((shell_refresh_pending&&shell_pending_key!=0x44)||
   (shell_refresh_commit&&shell_commit_action!=1));
}
uint64_t s865_shell_controller_event(uint32_t pid,uint64_t gen,uint64_t root){
 if(!shell_controller_auth(pid,gen,root)||!shell_refresh_pending)return 0;
 return (shell_controller.epoch<<32)|shell_pending_key;
}
uint64_t s865_shell_controller_decide(uint32_t pid,uint64_t gen,uint64_t root,uint64_t epoch,uint64_t action){
 if(!shell_controller_auth(pid,gen,root)||!shell_refresh_pending||shell_refresh_commit||
    epoch!=shell_controller.epoch||
    !((shell_pending_key==0x44&&action==1)||(shell_pending_key==0x3e&&action==2)||
      (shell_pending_key==0x42&&action==3)))return 0;
 shell_refresh_commit=epoch;shell_commit_action=(unsigned)action;
 shell_refresh_pending=false;shell_pending_key=0;return epoch;
}
uint64_t s865_shell_take_refresh(void){
 if(shell_commit_action!=1)return 0;
 uint64_t epoch=shell_refresh_commit;shell_refresh_commit=0;shell_commit_action=0;
 if(epoch){arch_puts("STYLEOS NATIVE SHELL-APPLY");field("epoch",epoch);arch_putc('\n');}
 return epoch;
}
unsigned s865_shell_take_command(void){
 if(!shell_refresh_commit||shell_commit_action<2||shell_recovery.pid)return 0;
 unsigned action=shell_commit_action;uint64_t epoch=shell_refresh_commit;
 shell_refresh_commit=0;shell_commit_action=0;
 arch_puts("STYLEOS NATIVE SHELL-ACTION");field("epoch",epoch);field("action",action);arch_putc('\n');
 return action;
}
uint64_t s865_shell_take_redraw(void){
 uint64_t epoch=shell_redraw;shell_redraw=0;
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
 if(epoch){arch_puts("STYLEOS NATIVE SHELL-REDRAW");field("epoch",epoch);arch_putc('\n');}
#endif
 return epoch;
}
/* Recovery policy is trusted and bounded; never re-enter a failed user
 * context. Retire a timed-out exact identity through the normal stop path.
 * A committed request does not need another instance. */
static void shell_recovery_tick(void){
 if(!shell_recovery.configured||shell_recovery.exhausted)return;
 if(shell_recovery.pid){
  if(!pr_find(pr_global(),shell_recovery.pid)){
   arch_puts("STYLEOS NATIVE SHELL-CONTROLLER-RETIRED");field("pid",shell_recovery.pid);
   field("epoch",shell_epoch);field("resident",pr_snapshot(pr_global(),0,0,true));arch_putc('\n');
   shell_recovery.pid=0;shell_recovery.gen=0;shell_recovery.slices=0;
   shell_recovery.delay=2u*shell_recovery.attempts;
  }else if(++shell_recovery.slices>=SHELL_SLICE_LIMIT){
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
   arch_puts("STYLEOS NATIVE SHELL-TIMEOUT");field("pid",shell_recovery.pid);
   field("gen",shell_recovery.gen);field("slices",shell_recovery.slices);
   field("dispatches",runtime860->dispatcher.dispatches);arch_putc('\n');
#endif
   if(s865_native_stop(shell_recovery.pid,shell_recovery.gen,true)!=PS_OK)fail("shell-recovery-timeout");
   shell_controller_reconcile();
   return;
  }else return;
 }
 if(!shell_refresh_pending)return;
 if(shell_recovery.delay){--shell_recovery.delay;return;}
 if(shell_recovery.attempts>=SHELL_ATTEMPTS){
  shell_recovery.exhausted=true;
  arch_puts("STYLEOS SHELL RECOVERY EXHAUSTED\n");return;
 }
 ++shell_recovery.attempts;
 uint32_t pid=0;
 enum ps_status status=s865_shell_controller_launch(&shell_recovery.request,&pid);
 if(status!=PS_OK){shell_recovery.delay=2u*shell_recovery.attempts;return;}
 shell_recovery.pid=pid;shell_recovery.gen=shell_controller.gen;shell_recovery.slices=0;
 arch_puts("STYLEOS NATIVE SHELL-RECOVERY");field("pid",pid);field("attempt",shell_recovery.attempts);
 field("dispatches",runtime860->dispatcher.dispatches);arch_putc('\n');
}
void s865_native_pump(void){
 if(!native_ready)fail("native-runtime-not-ready");
 shell_controller_reconcile();
 shell_recovery_tick();
 if(!pr_snapshot(pr_global(),0,0,true))return;
 s865_executor_poll(native_audit,native_release,0);
 shell_controller_reconcile();
 if(!pr_snapshot(pr_global(),0,0,true)){
  unsigned used;
  if(!s844_count(native_arena,&used)||used||!native_audit(0))fail("native-empty");
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
  arch_puts("STYLEOS NATIVE EMPTY");field("used",used);field("dispatches",runtime860->dispatcher.dispatches);arch_putc('\n');
#endif
 }
}


#ifdef STYLEOS_NATIVE_PINNED_CATALOG
static bool shell_gap_requested;
void s865_native_request_shell_gap(void){
 if(pr_snapshot(pr_global(),0,0,true)==2)shell_gap_requested=true;
}
#endif
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
#include "shell_controller_66.h"
static bool shell66_read(void *opaque,uint32_t sector,uint8_t out[512]){
 if(opaque||!out||sector>=sizeof shell66_image/512u)return false;
 for(unsigned i=0;i<512;++i)out[i]=shell66_image[sector*512u+i];
 return true;
}
static bool shell66_register(void){
 struct rt865_launch_request request={.source={.read=shell66_read,
  .sectors=sizeof shell66_image/512u,.bytes=SHELL66_BYTES},.name="Shell controller"};
 for(unsigned i=0;i<32;++i)request.source.sha256[i]=shell66_hash[i];
 if(s865_shell_configure_recovery(&request)!=PS_OK)return false;
 arch_puts("STYLEOS SHELL PROVIDER READY\n");return true;
}
#endif
_Noreturn void s865_native_supervise(void (*shell_step)(void *),void *state){
 if(!native_ready||!shell_step||!state||!s865_executor_idle()||!native_audit(0))
  fail("native-supervisor-entry");
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
 if(!shell66_register())fail("shell-provider-register");
#endif
 for(;;){
  shell_step(state);
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
  if(shell_gap_requested){
   shell_gap_requested=false;
   const unsigned slices=16;
   uint64_t start=runtime860->dispatcher.dispatches;
   arch_puts("STYLEOS NATIVE SHELL-GAP-BEGIN");field("dispatches",start);arch_putc('\n');
   for(unsigned i=0;i<slices;++i)s865_native_pump();
   arch_puts("STYLEOS NATIVE SHELL-GAP-END");field("dispatches",runtime860->dispatcher.dispatches);arch_putc('\n');
   if(!s865_native_probe_shell_controller())fail("shell-controller-probe");
  }
#endif
  s865_native_pump();
  for(unsigned i=0;i<64;++i)__asm__ volatile("pause");
 }
}
