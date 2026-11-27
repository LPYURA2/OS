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
 if(usr_bind(u,session,pid,gen)!=USR_OK||
    ws_open_app(w,registry,pid,title,bounds,&win)!=WS_OK||
    nwc_claim(n,pid,gen,root,win)!=NWC_OK){
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
void s865_native_pump(void){
 if(!native_ready)fail("native-runtime-not-ready");
 if(!pr_snapshot(pr_global(),0,0,true))return;
 s865_executor_poll(native_audit,native_release,0);
 if(!pr_snapshot(pr_global(),0,0,true)){
  unsigned used;
  if(!s844_count(native_arena,&used)||used||!native_audit(0))fail("native-empty");
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
  arch_puts("STYLEOS NATIVE EMPTY");field("used",used);field("dispatches",runtime860->dispatcher.dispatches);arch_putc('\n');
#endif
 }
}

