/* Ordinary native runtime: reuse the same audited detached-root and
 * hardware execution path without the historical batch driver. Included
 * only by the owned-root architecture after all root helpers are defined. */
#include "native_executor_865.h"
#include <styleos/native_runtime_865.h>
static struct s846_root native_roots[S846_ROOT_COUNT];
static struct s844_lease native_leases[S846_ROOT_COUNT];
static struct s846_extent native_extents[S846_ROOT_COUNT];
static struct s844_arena *native_arena;
static const struct boot_memory_evidence *native_evidence;
static struct s846_guard native_guard;
static struct s842_cache native_cache;
static bool native_ready;
static uint8_t native_scratch[8192];
static void native_wipe(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static bool native_extent_binding(void){
 /* A detached lease may precede admission, and an already terminal
  * bank slot may outlive its wiped root until synchronous bank release.
  * Every coexisting bank/lease identity must agree, not just page count. */
 for(unsigned i=0;i<S846_ROOT_COUNT;++i){
  const struct rt862_bank_slot *slot=&bank862.slots[i];
  if(!slot->pid)continue;
  if(!native_leases[i].generation){
   const struct pr_entry *p=pr_find(pr_global(),slot->pid);
   if(!p||(p->state!=PR_EXITED&&p->state!=PR_FAULTED))return false;
   continue;
  }
  struct s846_extent e=rt862_extent(&bank862,i),n=native_extents[i];
  if(slot->handle!=native_leases[i].generation||slot->root!=native_roots[i].built.root||
     e.code!=n.code||e.data!=n.data||e.stack!=n.stack)return false;
 }
 return true;
}
static bool native_audit(void *unused){
 (void)unused;
 return native_extent_binding()&&native_arena&&native_evidence&&read_cr3()==composed.built.root&&!(flags()&0x200)&&same_cache(&native_cache)&&
  rt875_roots_audit_sparse_frozen(base861,&native_cache,&frozen861,native_evidence,native_arena,native_leases,native_extents,&native_guard,&composed,native_roots);
}
/* Quiescent published-process accounting, distinct from the transitional
 * detached-root audit. No partial admission or terminal lease retirement
 * can cross the shell boundary. Validate the arena once, then compare
 * exact descriptors in its validated slots rather than re-scanning it
 * once per process. This is a metadata check, not a PTE audit substitute. */
static bool native_accounting(void){
 if(!native_ready||!s865_executor_idle()||!native_arena||
    !runtime860||runtime860->dispatcher.registry!=pr_global()||
    runtime860->dispatcher.current!=-1)return false;
 unsigned used=0,total=0,count=0;
 if(!s844_count(native_arena,&used)||!native_extent_binding())return false;
 for(unsigned i=0;i<S846_ROOT_COUNT;++i){
  const struct rt862_bank_slot *slot=&bank862.slots[i];
  const struct s844_lease l=native_leases[i];
  const struct s846_extent e=native_extents[i];
  if(!slot->pid){
   if(l.pa||l.owner||l.generation||l.pages||e.code||e.data||e.stack||
      native_roots[i].ready)return false;
   continue;
  }
  const struct pr_entry *p=pr_find(pr_global(),slot->pid);
  if(!p||p->type!=PR_NATIVE_USER||
     (p->state!=PR_READY&&p->state!=PR_WAITING)||
     p->scheduler_handle!=slot->handle||p->page_root!=slot->root||
     !e.code||e.code>NAI75_CODE_PAGES||!e.data||e.data>NAI75_DATA_PAGES||
     !e.stack||e.stack>NAI75_STACK_PAGES||l.pages!=e.code+e.data+e.stack||
     !l.generation||l.owner!=S846_OWNER_A+i||
     l.pa<native_arena->base||l.pa-native_arena->base>=S844_BYTES||
     (l.pa&4095u)||p->resident_bytes!=(uint64_t)l.pages*4096u||
     !native_roots[i].ready)return false;
  const struct s844_lease live=native_arena->slots[(l.pa-native_arena->base)/4096u];
  if(live.pa!=l.pa||live.pages!=l.pages||live.owner!=l.owner||
     live.generation!=l.generation)return false;
  for(unsigned j=0;j<i;++j)if(bank862.slots[j].pid==slot->pid||
     (native_leases[j].generation&&native_leases[j].generation==l.generation))return false;
  total+=l.pages;++count;
 }
 return total==used&&count==pr_snapshot(pr_global(),0,0,true);
}
volatile uint8_t *s865_native_data_span(unsigned id,uint32_t pid,uint64_t gen,uint64_t root,
                                        uint64_t va,size_t bytes){
 if(!native_ready||!bytes||va<S846_DATA||id>=S846_ROOT_COUNT||!runtime860||
    runtime860->dispatcher.current!=-1)return 0;
 /* Reject malformed spans before the costly tree audit. No authority is
  * inferred from this address calculation. */
 const struct s846_extent extent=native_extents[id];
 if(!extent.data||extent.data>16u||bytes>(size_t)extent.data*4096u||
    va-S846_DATA>(size_t)extent.data*4096u-bytes)return 0;
 const struct pr_entry *caller=pr_find(pr_global(),pid);
 const struct s844_lease lease=native_leases[id];
 if(!caller||caller->state!=PR_READY||caller->scheduler_handle!=gen||caller->page_root!=root||
    bank862.slots[id].pid!=pid||bank862.slots[id].handle!=gen||bank862.slots[id].root!=root||
    !native_roots[id].ready||native_roots[id].built.root!=root||
    lease.generation!=gen||lease.owner!=S846_OWNER_A+id||!native_audit(0))return 0;
 /* The audit verifies every PTE and all live leases; never dereference
  * the user VA. Only the caller's private RW/NX span may be aliased. */
 return (volatile uint8_t *)(uintptr_t)s875_data_pa(native_arena,lease,extent,va,bytes);
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
 native_wipe(&native_extents[id],sizeof native_extents[id]);
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
 native_extents[id]=(struct s846_extent){1,1,1};
 if(!s875_root_build(base861,&native_cache,native_arena,native_leases,native_extents,id,&native_guard,child_tables861[id],
    (uintptr_t)child_tables861[id],&native_roots[id])){
  native_wipe(&native_roots[id],sizeof native_roots[id]);
  native_wipe(child_tables861[id],sizeof child_tables861[id]);
  if(!s844_release(native_arena,native_leases[id].owner,native_leases[id]))fail("native-build-rollback");
  native_wipe(&native_leases[id],sizeof native_leases[id]);
  native_wipe(&native_extents[id],sizeof native_extents[id]);
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
/* Common detached allocation rollback. No process has been published and
 * no FP binding remains. The arena release scrubs every acquired page. */
static void native_discard_image(unsigned id){
 native_wipe(&native_roots[id],sizeof native_roots[id]);
 native_wipe(child_tables861[id],sizeof child_tables861[id]);
 if(!s844_release(native_arena,native_leases[id].owner,native_leases[id]))
    fail("875-image-discard");
 native_wipe(&native_leases[id],sizeof native_leases[id]);
 native_wipe(&native_extents[id],sizeof native_extents[id]);
 if(!native_audit(0))fail("875-image-discard-audit");
}
enum ps_status s875_native_launch_image(const uint8_t *elf,size_t bytes,const uint8_t hash[32],
 unsigned stack,const char *name,uint32_t *pid){
 if(!elf||!hash||!name||!pid||(uintptr_t)pid%_Alignof(uint32_t))return PS_ARGUMENT;
 if(!native_ready||!s865_executor_idle()||!native_audit(0))return PS_STATE;
 if(pr_global()->change_serial>UINT32_MAX-128u)return PS_EXHAUSTED;
 /* Copy the bounded name before any allocation. Source and output are
  * trusted supervisor objects, not user pointers. Reject arena aliasing
  * before zero-on-acquire can change even a vacant source span. */
 char label[PR_LABEL_BYTES]={0};
 unsigned n=0;for(;n<sizeof label&&name[n];++n)label[n]=name[n];
 if(!n||n==sizeof label)return PS_ARGUMENT;
 const uintptr_t start=(uintptr_t)elf,hp=(uintptr_t)hash,op=(uintptr_t)pid;
 const uint64_t lo=native_arena->base,hi=lo+S844_BYTES;
 if(!bytes||bytes>NAI75_MAX_ELF||bytes>UINTPTR_MAX-start||32u>UINTPTR_MAX-hp||
    sizeof *pid>UINTPTR_MAX-op||
    (start<hi&&lo<start+bytes)||(hp<hi&&lo<hp+32u)||(op<hi&&lo<op+sizeof *pid)||
    (op<start+bytes&&start<op+sizeof *pid)||(op<hp+32u&&hp<op+sizeof *pid))
    return PS_ARGUMENT;
 struct nai75_layout plan={0},staged={0};
 if(nai75_inspect(elf,bytes,hash,stack,&plan)!=NTE_OK)return PS_EVIDENCE;
 unsigned id=0;while(id<S846_ROOT_COUNT&&native_leases[id].generation)++id;
 if(id==S846_ROOT_COUNT)return PS_FULL;
 if(!s844_acquire(native_arena,S846_OWNER_A+id,plan.total_pages,&native_leases[id]))
    return PS_FULL;
 native_extents[id]=(struct s846_extent){plan.code_pages,plan.data_pages,plan.stack_pages};
 if(nai75_stage(native_arena,native_leases[id],elf,bytes,hash,stack,&staged)!=NTE_OK||
    staged.entry!=plan.entry||staged.code_pages!=plan.code_pages||
    staged.data_pages!=plan.data_pages||staged.stack_pages!=plan.stack_pages||
    staged.total_pages!=plan.total_pages||
    !s875_root_build(base861,&native_cache,native_arena,native_leases,native_extents,id,
       &native_guard,child_tables861[id],(uintptr_t)child_tables861[id],&native_roots[id])||
    !native_audit(0)){
    native_discard_image(id);return PS_EVIDENCE;
 }
 uint32_t assigned=0;
 enum ps_status status=s875_executor_admit(id,&native_roots[id],native_arena,
    &native_leases[id],&staged,label,native_audit,0,&assigned);
 if(status!=PS_OK){native_discard_image(id);return status;}
 if(pr_account(pr_global(),assigned,native_leases[id].generation,0,
               (uint64_t)native_leases[id].pages*4096u)!=PR_OK||
    !s865_executor_idle()||!native_audit(0))fail("875-image-publish");
 *pid=assigned;
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
enum ps_status s865_native_launch_tea(const uint8_t *package,size_t bytes,
 const struct nts_pin *pin,uint32_t session,const char *title,
 struct ws_bounds bounds,struct s865_window_launch *out)
{
 if(!package||!pin||!out||!title||!session||bytes!=pin->package_bytes||
    bytes<22u||bytes>NTP_MAX_PACKAGE||!pin->package_sha||!pin->elf_sha||
    !pin->manifest||!pin->manifest_bytes||pin->manifest_bytes>4096u)
  return PS_ARGUMENT;
 if(!native_ready||!s865_executor_idle()||!native_audit(0))return PS_STATE;
 struct ntp_payload view={0};
 if(ntp_open(package,bytes,pin->package_sha,pin->elf_sha,pin->manifest,
             pin->manifest_bytes,&view)!=NTP_OK)return PS_EVIDENCE;
 if(view.elf_bytes<120u||view.elf_bytes>8192u)return PS_EVIDENCE;
 struct rt865_launch_request request={0};
 unsigned i=0;
 for(;i<sizeof request.name&&title[i];++i){
  unsigned char c=(unsigned char)title[i];
  if(c<0x20||c==0x7f)return PS_ARGUMENT;
  request.name[i]=title[i];
 }
 if(!i||i==sizeof request.name)return PS_ARGUMENT;
 request.source=(struct s865_image_source){.read=ntp_elf_sector,.opaque=&view,
  .lba=0,.sectors=(uint32_t)((view.elf_bytes+511u)/512u),.bytes=(uint32_t)view.elf_bytes};
 for(i=0;i<32;++i)request.source.sha256[i]=pin->elf_sha[i];
 /* Existing audited launch rechecks ELF rights and digest, then binds
  * session, window and port before any dispatch. The stack view is never
  * retained by the scheduler or published into user memory. */
 return s865_native_launch_window(&request,session,title,bounds,out);
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
 if(!e||(e->state!=PR_READY&&e->state!=PR_WAITING))return PS_STATE;
 return s865_native_stop(pid,e->scheduler_handle,true);
}
enum pa_status s865_native_snapshot(const struct pa_request *q,struct pa_row *rows,
                                    size_t capacity,struct pa_reply *reply){
 /* Exact live per-process page accounting and full actual PTE audit
  * before publishing measurements. Never fabricate CPU percentages. */
 if(!native_accounting()||!native_audit(0))return PA_DENIED;
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
 if(key!=0x3e&&key!=0x42&&!(key>=0x101&&key<=0x17e)&&key!=0x200&&key!=0x201&&!(key>=0x301&&key<=0x30c))return false;
 return shell_submit_key(key);
#endif
}
bool s865_shell_command_waiting(void){
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
 return !shell_recovery.exhausted&&((shell_refresh_pending&&shell_pending_key!=0x44)||
   (shell_refresh_commit&&shell_commit_action!=1));
#else
 return !shell_recovery.exhausted&&(shell_refresh_pending||shell_refresh_commit);
#endif
}
uint64_t s865_shell_controller_event(uint32_t pid,uint64_t gen,uint64_t root){
 if(!shell_controller_auth(pid,gen,root)||!shell_refresh_pending)return 0;
 return (shell_controller.epoch<<32)|shell_pending_key;
}
/* Kernel checks the scope of a decision, independently of ELF policy.
 * The target window and pointer coordinates stay in trusted view state. */
static unsigned shell_allowed_action(unsigned key){
 if(key==0x44)return 1;
 if(key==0x3e)return 2;
 if(key==0x42)return 3;
 if(key>=0x101&&key<0x12a)return 4;
 if(key>=0x12a&&key<0x154)return 5;
 if(key>=0x154&&key<0x17e)return 6;
 if(key==0x17e)return 7;
 if(key==0x200)return 6;
 if(key==0x201)return 8;
 if(key>=0x301&&key<=0x30c)return key-0x301+9;
 return 0;
}
uint64_t s865_shell_controller_decide(uint32_t pid,uint64_t gen,uint64_t root,uint64_t epoch,uint64_t action){
 if(!shell_controller_auth(pid,gen,root)||!shell_refresh_pending||shell_refresh_commit||
    epoch!=shell_controller.epoch||!action||action!=shell_allowed_action(shell_pending_key))return 0;
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
#include "native_sample_69.h"
/* Immutable, individually approved preinstalled packages. No shared user
 * address spaces, mutable package bytes or retained stack reader state. */
struct sample69_entry {
 const uint8_t *package;
 size_t bytes;
 struct nts_pin pin;
 const char *name;
 struct ws_bounds bounds;
};
#define SAMPLE69(n,source,label,x,y) {source,sample69_##n##_bytes, \
 {sample69_##n##_pkg_hash,sample69_##n##_elf_hash,sample69_##n##_manifest, \
 sizeof sample69_##n##_manifest,sample69_##n##_bytes},label,{x,y,160,120}}
static const struct sample69_entry sample69_catalog[]={
 SAMPLE69(0,sample69_0_package,"Native-sample",80,80),
 SAMPLE69(1,0,"Native-peer",280,80)
};
#undef SAMPLE69
#define SAMPLE69_COUNT (sizeof sample69_catalog/sizeof sample69_catalog[0])
static struct s865_window_launch sample69_live[SAMPLE69_COUNT];
static uint32_t sample69_session;
/* Explicitly opted-in immutable sample registry only. Other native callers
 * retain nonblocking INPUT semantics; no arbitrary binary opt-in by name. */
bool s865_sample_wait_allowed(uint32_t pid,uint64_t gen,uint64_t root){
 const struct pr_entry *e=pr_find(pr_global(),pid);
 if(!e||e->scheduler_handle!=gen||e->page_root!=root)return false;
 for(unsigned i=0;i<SAMPLE69_COUNT;++i)
  if(sample69_live[i].pid==pid)return true;
 return false;
}
#ifdef STYLEOS_LIVE_VOLUME_39
#include <styleos/native_tea_stylefs.h>
/* Only the mounted volume owner may resolve the exact file revision. */
bool s69_use_tea_file(const struct po64_row *,const struct nts_pin *,ntsfs_consumer,void *);
#endif
/* Narrow legacy provisioned media extent, never a raw probe of StyleFS
 * volumes. The approved digest and exact size reside in the boot image.
 * Temporary package storage is bounded and scrubbed after use. */
#if defined(STYLEOS_PROVISIONED_STORAGE_864) && !defined(STYLEOS_LIVE_VOLUME_39) && !defined(STYLEOS_STYLEFS_MOUNT_RO) && !defined(STYLEOS_STYLEFS_MOUNT_RW) && !defined(STYLEOS_STYLEFS_DESKTOP_SCOPE) && !defined(STYLEOS_LBA48_BLOCK_19) && !defined(STYLEOS_WIDE_GUEST_20) && !defined(STYLEOS_PAGED_GUEST_34)
bool s863_disk_read(uint32_t,uint8_t[512]);
static uint8_t sample69_disk_package[NTP_MAX_PACKAGE];
#include "deep_drop_pins.h"
/* Optional external acceptance media. Both user programs are loaded through
 * the normal pinned package loader. No executable bytes are compiled in. */
#define DROP72_PIN(role) {drop72_##role##_pkg_hash,drop72_##role##_elf_hash, \
 drop72_##role##_manifest,sizeof drop72_##role##_manifest,drop72_##role##_bytes}
static const struct sample69_entry drop72_catalog[]={
 {0,drop72_source_bytes,DROP72_PIN(source),"Drop-source",{80,80,160,120}},
 {0,drop72_target_bytes,DROP72_PIN(target),"Drop-target",{280,80,160,120}}
};
#undef DROP72_PIN
static enum ps_status sample_disk_launch(unsigned index,struct s865_window_launch *out){
 const uint32_t lba=index?3000u:2960u;
 const struct sample69_entry *app=&drop72_catalog[index];
 uint8_t sector[512];uint32_t bytes=0;
 bool header=s863_disk_read(lba,sector);
 if(!header || ntp_disk_header(sector,app->pin.package_sha,&bytes)!=NTP_OK){
  app=&sample69_catalog[index];
  if(app->package)return s865_native_launch_tea(app->package,app->bytes,&app->pin,
      sample69_session,app->name,app->bounds,out);
  if(!header||ntp_disk_header(sector,app->pin.package_sha,&bytes)!=NTP_OK)return PS_EVIDENCE;
 }
 if(bytes!=app->bytes||bytes>NTP_MAX_PACKAGE)return PS_EVIDENCE;
 enum ps_status result=PS_EVIDENCE;
 for(uint32_t offset=0;offset<bytes;offset+=512u){
  if(!s863_disk_read(lba+1u+offset/512u,sector))goto done;
  uint32_t count=bytes-offset<512u?bytes-offset:512u;
  for(uint32_t j=0;j<count;++j)sample69_disk_package[offset+j]=sector[j];
 }
 result=s865_native_launch_tea(sample69_disk_package,bytes,&app->pin,sample69_session,
                               app->name,app->bounds,out);
 if(result==PS_OK){
  arch_puts(app==&drop72_catalog[index]?"STYLEOS DD72 DISK":"STYLEOS TEA69 DISK");
  field("bytes",bytes);field("sectors",1u+(bytes+511u)/512u);arch_putc('\n');
 }
done:
 native_wipe(sample69_disk_package,bytes);
 return result;
}
#endif
static bool sample69_session_ready(void){
 struct usr_runtime *u=(struct usr_runtime *)usr_global();
 if(!u||!u->ready)return false;
 /* No login yet: local transient session, created only once. Never unlock
  * or substitute another user's selected session. */
 if(!u->selected_session){
  if(sample69_session||u->next_session!=1)return false;
  if(usr_create(u,1,&sample69_session)!=USR_OK||
     usr_select(u,sample69_session)!=USR_OK)return false;
 }
 return u->selected_session==sample69_session;
}
/* Count actual native leases, not VM RAM or estimated process sizes.
 * Kernel image, reserved tables, graphics and caches are outside this metric. */
/* Boot map is diagnostic: conventional candidates have NOT been granted
 * to a general allocator. Protected ranges may include MMIO and firmware.
 * Only report non-overlapping, independently attested image/arena extents. */
static void sample70_memory(unsigned phase,uint32_t pid,unsigned used){
 const struct boot_memory_map *m=native_evidence->map;
 struct boot_range image=native_evidence->live.image;
 const uint64_t mask=BOOT_MEMORY_PAGE-1;
 if(!m||image.end<=image.start||image.end>UINT64_MAX-mask||used>S844_PAGES||
    native_arena->base>UINT64_MAX-S844_BYTES)fail("sample70-memory-map");
 image.start&=~mask;image.end=(image.end+mask)&~mask;
 uint64_t arena_end=native_arena->base+S844_BYTES;
 if(image.start<arena_end&&native_arena->base<image.end)fail("sample70-memory-overlap");
 uint64_t candidates=0;
 if(m->nr_candidates>BOOT_MEMORY_RANGE_MAX)fail("sample70-memory-count");
 for(size_t i=0;i<m->nr_candidates;++i){
  struct boot_range x=m->candidates[i];
  if(x.end<=x.start||(x.start&mask)||(x.end&mask)||
     (i&&x.start<m->candidates[i-1].end)||
     candidates>UINT64_MAX-(x.end-x.start)||
     (x.start<image.end&&image.start<x.end)||
     (x.start<arena_end&&native_arena->base<x.end))fail("sample70-memory-candidates");
  candidates+=x.end-x.start;
 }
 arch_puts("STYLEOS TEA70 MEMORY");field("phase",phase);field("pid",pid);
 field("image",image.end-image.start);field("arena",S844_BYTES);
 field("leased",(uint64_t)used*BOOT_MEMORY_PAGE);
 field("arena-free",(uint64_t)(S844_PAGES-used)*BOOT_MEMORY_PAGE);
 field("ungranted",candidates);arch_putc('\n');
}
static void sample70_resources(unsigned phase,uint32_t pid){
 unsigned used=0,owners=0;
 const struct usr_runtime *u=usr_global();
 const struct nwc_service *ports=nwc_global();
 const struct ws_service *windows=ws_global();
 if(!u||!ports||!windows||!native_audit(0)||!s844_count(native_arena,&used))fail("sample70-resource-audit");
 size_t resident=pr_snapshot(pr_global(),0,0,true);
 for(unsigned i=0;i<PR_MAX_PROCESSES;++i)if(u->owners[i].pid)++owners;
 if(used!=3u*resident)fail("sample70-page-account");
 arch_puts("STYLEOS TEA70 RESOURCES");field("phase",phase);field("pid",pid);
 field("resident",resident);field("pages",used);field("owners",owners);
 field("windows",ws_snapshot(windows,0,0));field("ports",nwc_port_count(ports));arch_putc('\n');
 sample70_memory(phase,pid,used);
}
static bool sample69_publish(unsigned index,struct s865_window_launch out){
 const struct pr_entry *e=pr_find(pr_global(),out.pid);
 if(!e||!e->scheduler_handle||!e->page_root)fail("sample69-owner");
 sample69_live[index]=out;
 arch_puts("STYLEOS TEA69 LAUNCHED");field("pid",out.pid);field("gen",e->scheduler_handle);
 field("root",e->page_root);field("window",out.window);arch_putc('\n');
 sample70_resources(1,out.pid);return true;
}
bool s865_launch_native_sample(void){
 if(!native_ready||!s865_executor_idle()||!native_audit(0))return false;
 unsigned index=0;
 while(index<SAMPLE69_COUNT&&sample69_live[index].pid)++index;
 if(index==SAMPLE69_COUNT||!sample69_session_ready())return false;
 struct s865_window_launch out={0};
 enum ps_status status=PS_EVIDENCE;
#if defined(STYLEOS_PROVISIONED_STORAGE_864) && !defined(STYLEOS_LIVE_VOLUME_39) && !defined(STYLEOS_STYLEFS_MOUNT_RO) && !defined(STYLEOS_STYLEFS_MOUNT_RW) && !defined(STYLEOS_STYLEFS_DESKTOP_SCOPE) && !defined(STYLEOS_LBA48_BLOCK_19) && !defined(STYLEOS_WIDE_GUEST_20) && !defined(STYLEOS_PAGED_GUEST_34)
 status=sample_disk_launch(index,&out);
#else
 const struct sample69_entry *a=&sample69_catalog[index];
 if(a->package)status=s865_native_launch_tea(a->package,a->bytes,&a->pin,sample69_session,
    a->name,a->bounds,&out);
#endif
 if(status!=PS_OK)return false;
 return sample69_publish(index,out);
}
#ifdef STYLEOS_LIVE_VOLUME_39
struct sample69_file_launch {const struct sample69_entry *app;struct s865_window_launch out;};
static bool sample69_consume_file(void *opaque,const uint8_t *bytes,size_t size){
 struct sample69_file_launch *c=opaque;
 if(!sample69_session_ready())return false;
 return s865_native_launch_tea(bytes,size,&c->app->pin,sample69_session,
   c->app->name,c->app->bounds,&c->out)==PS_OK;
}
bool s865_launch_native_file(const struct po64_row *row){
 static const char name[]="Native-peer.tea";
 const unsigned index=1;
 const struct sample69_entry *a=&sample69_catalog[index];
 if(!row||row->entry.kind!=SD64_FILE||row->entry.key.length!=sizeof name-1||
    row->bytes!=a->bytes||sample69_live[index].pid||
    !native_ready||!s865_executor_idle()||!native_audit(0))return false;
 for(size_t i=0;i<sizeof name-1;++i)
  if(row->entry.key.name[i]!=(uint8_t)name[i])return false;
 struct sample69_file_launch c={.app=a};
 if(!s69_use_tea_file(row,&a->pin,sample69_consume_file,&c))return false;
 arch_puts("STYLEOS TEA69 STYLEFS");field("id",row->entry.target);
 field("revision",row->revision);field("bytes",row->bytes);arch_putc('\n');
 return sample69_publish(index,c.out);
}
#endif
static void sample69_reconcile(void){
 for(unsigned i=0;i<SAMPLE69_COUNT;++i){
  struct s865_window_launch *a=&sample69_live[i];
  if(!a->pid||pr_find(pr_global(),a->pid))continue;
  const struct nwc_service *n=nwc_global();
  if(ws_find(ws_global(),a->window)||!n||!native_audit(0))fail("sample69-retire");
  for(unsigned j=0;j<NWC_PORTS;++j)
   if(n->ports[j].window_id==a->window||n->ports[j].owner_pid==a->pid)
    fail("sample69-port-retire");
  arch_puts("STYLEOS TEA69 RETIRED");field("pid",a->pid);field("window",a->window);arch_putc('\n');
  sample70_resources(2,a->pid);
  *a=(struct s865_window_launch){0};
 }
}
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
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
  sample69_reconcile();
#endif
  for(unsigned i=0;i<64;++i)__asm__ volatile("pause");
 }
}
