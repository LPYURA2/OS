#include <styleos/runtime_entry_858.h>
#include <styleos/desktop_process_runtime.h>
#include <styleos/user_session_runtime.h>
#include <styleos/native_window_channel.h>
#define BIT(n) (UINT64_C(1)<<(n))
static unsigned phase;
static uint64_t active_root;
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
bool rt858_start(const struct rt858_expected *e,const struct rt858_live *v){
 if(phase||!rt858_validate(e,v))return false;
 phase=1; /* sticky refusal on any partial startup */
 if(pr_snapshot(pr_global(),0,0,true)||ws_snapshot(ws_global(),0,0)||dpr_global_state())return false;
 if(dpr_global_start(e->root)!=DPR_OK||usr_global_start()!=USR_OK||nwc_global_start()!=NWC_OK||!empty(e->root))return false;
 active_root=e->root;phase=2;return true;
}
bool rt858_ready(uint64_t root){return phase==2&&root==active_root&&empty(root);}
