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


#ifdef STYLEOS_FILE_GUEST_45
#include <styleos/stylefs_file_syscall43.h>
static const struct pf64_service *file_service45;
static struct vm64_mount *file_mount45;
static struct wv64_workspace *file_work45;
static bool file_audit45(void *unused,const struct rt862_selection *t,const uint8_t *page){
 (void)unused;
 if(!t||t->slot>=S846_ROOT_COUNT||!native_audit(0)||t->safe_root!=read_cr3()||
    t->child_root!=native_roots[t->slot].built.root)return false;
 const struct s844_lease l=native_leases[t->slot];
 if(l.generation!=t->handle||l.owner!=S846_OWNER_A+t->slot||l.pages!=3||
    !s844_lease_live(native_arena,l)||(uintptr_t)page!=l.pa+PG_PAGE)return false;
 /* Native audit already reconstructs both actual child and composed
  * desktop root, including supervisor-only arena identity mapping. */
 return identity(l.pa+PG_PAGE,PG_WRITE);
}
enum ps_status s45_file_event(const struct rt862_selection *t,const struct rt860_tick *event,uint64_t cr2){
 if(!file_service45||!file_mount45||!file_work45||!t||!event||
    t->slot>=S846_ROOT_COUNT||event->vector!=128||event->isr||event->eoi||
    event->slot!=t->slot||event->pid!=t->pid||event->handle!=t->handle||
    event->dispatch!=t->dispatch||event->child_root!=t->child_root||
    event->safe_root!=read_cr3())return PS_EVIDENCE;
 const struct fs43_binding b={file_service45,file_mount45,file_work45,
 (uint8_t *)(uintptr_t)(native_leases[t->slot].pa+PG_PAGE),file_audit45,0};
 return rt43_file_read(&bank862,runtime860,t,read_cr3(),flags(),&event->frame,event->frame_va,cr2,&b);
}
bool s45_file_cleanup(void){
 return !file_service45||pf64_sweep(file_service45)==WV64_OK;
}
#endif
