#include <styleos/runtime_entry_858.h>
#include <styleos/desktop_process_runtime.h>
#include <styleos/user_session_runtime.h>
#include <styleos/native_window_channel.h>
#define BIT(n) (UINT64_C(1)<<(n))
static unsigned phase, bootstrap_phase;
static uint64_t bootstrap_root;
static uint64_t active_root;
static struct rt858_expected started_contract;
#if (defined(STYLEOS_TEST_RUNTIME_JOIN_861) || defined(STYLEOS_OWNED_SHELL_865))
#include <styleos/runtime_tick_860.h>
#endif
static bool page(uint64_t v){return v && !(v&4095u) && v<BIT(47);}
bool rt858_validate(const struct rt858_expected *e,const struct rt858_live *v){
 return e&&v&&page(e->root)&&page(e->old_root)&&e->root!=e->old_root&&
        page(e->gdt)&&page(e->idt)&&e->gdt!=e->idt&&e->gdt!=e->root&&e->idt!=e->root&&
        e->stack_lo&&!(e->stack_lo&15u)&&e->stack_hi>e->stack_lo&&e->stack_hi<BIT(47)&&!(e->stack_hi&15u)&&
        e->cs&&!(e->cs&7u)&&e->tr>e->cs&&!(e->tr&7u)&&e->tr<=4080&&e->gdt_limit==e->tr+15&&
        v->root==e->root&&v->gdt==e->gdt&&v->idt==e->idt&&v->rsp>=e->stack_lo&&v->rsp<e->stack_hi&&
        v->gdt_limit==e->gdt_limit&&v->idt_limit==4095&&v->tr==e->tr&&v->cs==e->cs&&
        (v->cr0&(BIT(31)|BIT(16)|BIT(0)))==(BIT(31)|BIT(16)|BIT(0))&&
        (v->cr4&BIT(5))&&!(v->cr4&(BIT(7)|BIT(12)|BIT(17)))&&
        (v->efer&(BIT(10)|BIT(11)))==(BIT(10)|BIT(11))&&(v->rflags&2)&&!(v->rflags&(BIT(9)|BIT(14)|BIT(17)));
}
static bool empty(uint64_t root){
 const struct dpr_service *d=dpr_global_state();const struct usr_runtime *u=usr_global();const struct nwc_service *n=nwc_global();
 if(!d||!u||!n||d->dispatcher.safe_root!=root||d->dispatcher.registry!=pr_global()||d->windows!=ws_global()||d->dispatcher.current!=-1||d->dispatcher.dispatches||d->dispatcher.next||pr_snapshot(pr_global(),0,0,true)||ws_snapshot(ws_global(),0,0)||!u->ready||u->processes!=pr_global()||u->windows!=ws_global()||u->selected_session||u->next_session!=1||!n->ready||n->registry!=pr_global()||n->sessions!=u||n->windows!=ws_global()||nwc_port_count(n))return false;
 for(unsigned i=0;i<USR_MAX_SESSIONS;i++)if(u->sessions[i].id||u->sessions[i].account_id||u->sessions[i].state!=USR_CLOSED)return false;
 for(unsigned i=0;i<PR_MAX_PROCESSES;i++)if(u->owners[i].pid||u->owners[i].session_id||u->owners[i].scheduler_handle)return false;
 for(unsigned i=0;i<NWC_PORTS;i++)if(n->ports[i].window_id||n->ports[i].owner_pid||n->ports[i].length||n->ports[i].has_tile)return false;
 for(unsigned i=0;i<PS_MAX_SLOTS;i++)if(d->dispatcher.slots[i].pid||d->dispatcher.slots[i].handle||d->dispatcher.slots[i].user_root)return false;
 return true;
}
/* Shared cold-start contract for both ordinary and owned-root Desktop.
 * No CR3 switch, device access, task admission or hardware attestation here. */
static bool zero_bytes(const void *p,size_t n){
 const unsigned char *b=p;for(size_t i=0;i<n;i++)if(b[i])return false;return true;
}
static bool pristine(void){
 const struct pr_table *p=pr_global();const struct ws_service *w=ws_global();
 return p->next_pid==1&&!p->change_serial&&w->next_id==1&&w->compositor_generation==1&&
  zero_bytes(p->processes,sizeof p->processes)&&zero_bytes(w->windows,sizeof w->windows);
}
bool rt863_bootstrap(uint64_t root){
 if(bootstrap_phase||!page(root))return false;
 bootstrap_phase=1; /* a valid-root attempt is one-shot, including refusal */
 if(dpr_global_state()||usr_global()||nwc_global()||!pristine())return false;
 if(dpr_global_start(root)!=DPR_OK||usr_global_start()!=USR_OK||nwc_global_start()!=NWC_OK||!empty(root)||!pristine())return false;
 bootstrap_root=root;bootstrap_phase=2;return true;
}
bool rt863_bootstrap_ready(uint64_t root){
 return bootstrap_phase==2&&root==bootstrap_root&&empty(root)&&pristine();
}
bool rt858_start(const struct rt858_expected *e,const struct rt858_live *v){
 if(phase||bootstrap_phase||!rt858_validate(e,v))return false;
 phase=1; /* sticky refusal on any partial startup */
 if(!rt863_bootstrap(e->root))return false;
 active_root=e->root;
 started_contract=*e;
 phase=2;return true;
}
bool rt858_ready(uint64_t root){return phase==2&&root==active_root&&rt863_bootstrap_ready(root);}

/* A parked native context remains live and retains its existing capabilities.
 * The bank and architecture separately attest saved context and actual wait
 * conditions. RUNNING is never permitted at this quiescent boundary. */
static bool boundary_live(enum pr_state s){return s==PR_READY||s==PR_WAITING;}
/* Shared ongoing service-graph validation. The root comes from the trusted
 * architecture, not from an app. Unlike initial-ready this allows admitted
 * native tasks and completed lifecycle counters. It never activates a root,
 * exposes an app ABI, or authorizes a hardware dispatch. */
bool rt864_services_boundary(uint64_t live_root){
 if(bootstrap_phase!=2||!page(live_root)||live_root!=bootstrap_root)return false;
 const struct dpr_service *d=dpr_global_state();
 const struct usr_runtime *u=usr_global();const struct nwc_service *n=nwc_global();
 if(!d||!d->started||!u||!u->ready||!n||!n->ready||
    d->dispatcher.safe_root!=live_root||d->dispatcher.registry!=pr_global()||d->windows!=ws_global()||
    u->processes!=pr_global()||u->windows!=ws_global()||n->registry!=pr_global()||n->windows!=ws_global()||n->sessions!=u||
    d->dispatcher.current!=-1||d->dispatcher.next>=PS_MAX_SLOTS)return 0;
 const struct ps_dispatcher *ps=&d->dispatcher;
 /* Native user contexts only; do not invent kernel tasks from UI sessions. */
 for(unsigned i=0;i<PS_MAX_SLOTS;i++){
  const struct ps_slot *s=&ps->slots[i];
  if(!s->pid){if(s->handle||s->user_root)return 0;continue;}
  const struct pr_entry *p=pr_find(ps->registry,s->pid);
  if(!p||p->type!=PR_NATIVE_USER||!s->handle||!page(s->user_root)||s->user_root==live_root||
     p->scheduler_handle!=s->handle||p->page_root!=s->user_root||
     (!boundary_live(p->state)&&p->state!=PR_EXITED&&p->state!=PR_FAULTED))return 0;
  for(unsigned j=0;j<i;j++)if(ps->slots[j].pid&&(ps->slots[j].pid==s->pid||ps->slots[j].handle==s->handle||ps->slots[j].user_root==s->user_root))return 0;
 }
 for(unsigned i=0;i<PR_MAX_PROCESSES;i++){
  const struct pr_entry *p=&ps->registry->processes[i];if(p->state==PR_UNUSED)continue;
  unsigned matches=0;for(unsigned j=0;j<PS_MAX_SLOTS;j++)if(ps->slots[j].pid==p->pid&&p->pid)matches++;
  if(matches!=1||p->type!=PR_NATIVE_USER||(!boundary_live(p->state)&&p->state!=PR_EXITED&&p->state!=PR_FAULTED))return 0;
  for(unsigned j=0;j<i;j++)if(ps->registry->processes[j].state!=PR_UNUSED&&ps->registry->processes[j].pid==p->pid)return 0;
 }
 /* Validate existing user/window capabilities at every safe point. A missing
  * optional binding is allowed; a stale or duplicated binding never is. */
 unsigned active=0;
 for(unsigned i=0;i<USR_MAX_SESSIONS;i++){
  const struct usr_session *s=&u->sessions[i];
  if(!s->id){if(s->account_id||s->state!=USR_CLOSED)return false;continue;}
  if(!s->account_id||s->id>=u->next_session||(s->state!=USR_ACTIVE&&s->state!=USR_LOCKED))return false;
  if(s->state==USR_ACTIVE){if(s->id!=u->selected_session)return false;++active;}
  for(unsigned j=0;j<i;j++)if(u->sessions[j].id&&(u->sessions[j].id==s->id||u->sessions[j].account_id==s->account_id))return false;
 }
 if(!u->next_session||active!=(u->selected_session?1u:0u))return false;
 for(unsigned i=0;i<PR_MAX_PROCESSES;i++){
  const struct usr_owner *o=&u->owners[i];
  if(!o->pid){if(o->session_id||o->scheduler_handle)return false;continue;}
  const struct pr_entry *p=pr_find(ps->registry,o->pid);
  if(!p||!boundary_live(p->state)||p->scheduler_handle!=o->scheduler_handle||!usr_find(u,o->session_id))return false;
  for(unsigned j=0;j<i;j++)if(u->owners[j].pid&&(u->owners[j].pid==o->pid||u->owners[j].scheduler_handle==o->scheduler_handle))return false;
 }
 for(unsigned i=0;i<NWC_PORTS;i++){
  const struct nwc_port *q=&n->ports[i];
  if(!q->window_id){if(!zero_bytes(q,sizeof *q))return false;continue;}
  const struct pr_entry *p=pr_find(ps->registry,q->owner_pid);
  const struct ws_window *w=ws_find(n->windows,q->window_id);
  if(!p||!boundary_live(p->state)||p->page_root!=q->user_root||p->scheduler_handle!=q->scheduler_handle||!w||w->role!=WS_APP_WINDOW||w->owner_pid!=q->owner_pid||!q->next_sequence||q->head>=NWC_EVENTS||q->length>NWC_EVENTS||q->length>=q->next_sequence)return false;
  unsigned matches=0;
  for(unsigned j=0;j<PR_MAX_PROCESSES;j++)if(u->owners[j].pid==q->owner_pid&&u->owners[j].session_id==q->session_id&&u->owners[j].scheduler_handle==q->scheduler_handle)++matches;
  if(matches!=1)return false;
  for(unsigned j=0;j<i;j++)if(n->ports[j].window_id==q->window_id)return false;
  for(unsigned j=0;j<q->length;j++){
   const struct nwc_event *e=&q->events[(q->head+j)%NWC_EVENTS];
   if(e->window_id!=q->window_id||e->sequence!=q->next_sequence-q->length+j||e->size>NWC_TEXT_BYTES)return false;
   if(e->kind==NWC_POINTER){
    if(e->size!=8||(e->code&~7u)||(e->modifiers&~15u)||e->down!=((e->code&1u)!=0))return false;
    for(unsigned k=8;k<NWC_TEXT_BYTES;++k)if(e->bytes[k])return false;
   }else if(e->kind==NWC_DEEP_DROP){
    if(!nwc_drop_event_valid(e))return false;
   }else if(e->kind!=NWC_KEY&&e->kind!=NWC_TEXT)return false;
  }
  if(!nwc_frame_valid(q,w))return false;
  if(q->has_tile&&(!q->tile.generation||q->tile.window_id!=q->window_id||!q->tile.width||!q->tile.height||q->tile.width>NWC_TILE_WIDTH||q->tile.height>NWC_TILE_HEIGHT))return false;
 }
 return true;
}
/* Shared shell boundary: an inherited root cannot carry native dispatch
 * authority. An owned root must present a fresh privileged CPU snapshot.
 * Neither path grants mappings, device access, or new hardware authority. */
bool rt864_shell_boundary(uint64_t root,const struct rt858_live *live){
 if(!rt864_services_boundary(root))return false;
 if(!live)return !phase&&rt863_bootstrap_ready(root);
 return phase==2&&active_root==root&&live->root==root&&
        rt858_validate(&started_contract,live);
}
bool rt864_shell_checkpoint(const struct rt858_live *v){
 if(!v||!(v->cr0&BIT(31))||!(v->cr4&BIT(5))||!(v->efer&BIT(10)))return false;
 if(phase)return rt864_shell_boundary(v->root,v);
 /* Preserve the inherited CR3 cache/PCID-bit convention without granting
  * owned-root authority; a failed owned contract cannot fall back here. */
 return rt864_shell_boundary(v->root&~UINT64_C(4095),0);
}
#if (defined(STYLEOS_TEST_RUNTIME_JOIN_861) || defined(STYLEOS_OWNED_SHELL_865))
/* The owned-root adapter additionally requires its original descriptor and
 * stack contract. The common software boundary does not replace that proof. */
struct dpr_service *rt861_dispatcher(const struct rt858_live *live){
 if(!live||!rt864_shell_boundary(live->root,live))return 0;
 struct dpr_service *d=dpr_tick_global_860();
 return d==dpr_global_state()?d:0;
}
#endif
