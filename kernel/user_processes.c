#include "styleos/user_processes.h"
#include <limits.h>

static void clear(void *p,size_t n)
{
    unsigned char *b=p;for(size_t i=0;i<n;++i)b[i]=0;
}
static int blank(const void *p,size_t n)
{
    const unsigned char *b=p;for(size_t i=0;i<n;++i)if(b[i])return 0;return 1;
}
static int pointer(const void *p,size_t bytes,size_t alignment)
{
    uintptr_t a=(uintptr_t)p;return p && !(a%alignment) && a<=UINTPTR_MAX-bytes;
}
static int external(const struct up_control *c,const void *p,size_t bytes,size_t alignment)
{
    if(!pointer(p,bytes,alignment))return 0;
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)c;
    return !(a<b+sizeof *c && b<a+bytes);
}
static int overlap(struct boot_range a,struct boot_range b)
{
    return a.start<b.end && b.start<a.end;
}
static enum up_error spec_valid(const struct up_spec *s,uint64_t safe)
{
    if(!s->root || s->root==safe || (s->root&(UP_PAGE-1u)))return UP_ALIGNMENT;
    if((s->code_va|s->data_va|s->stack_va.start|s->stack_va.end)&(UP_PAGE-1u))return UP_ALIGNMENT;
    if(s->code_va>UINT64_MAX-UP_PAGE || s->data_va>UINT64_MAX-UP_PAGE ||
       s->stack_va.start>UINT64_MAX-UP_PAGE)return UP_OVERFLOW;
    if(s->stack_va.end-s->stack_va.start!=UP_PAGE ||
       s->entry<s->code_va || s->entry-s->code_va>=UP_PAGE)return UP_RANGE;
    struct boot_range code={s->code_va,s->code_va+UP_PAGE};
    struct boot_range data={s->data_va,s->data_va+UP_PAGE};
    if(overlap(code,data)||overlap(code,s->stack_va)||overlap(data,s->stack_va))return UP_OVERLAP;
    return UP_OK;
}
static enum up_error valid(const struct up_control *c)
{
    if(!pointer(c,sizeof *c,_Alignof(struct up_control)))return UP_ARGUMENT;
    if(!c->safe_root || (c->safe_root&(UP_PAGE-1u)) || c->current < -1 ||
       c->current>=(int)UP_SLOTS || c->next>=UP_SLOTS || c->phase>4 ||
       c->dispatches>4 || c->released>UP_SLOTS)return UP_STATE;
    unsigned running=0;
    for(unsigned i=0;i<UP_SLOTS;++i){
        const struct up_slot *s=&c->slots[i];
        if(s->state==UP_FREE){
            if(!blank(&s->spec,sizeof s->spec)||!blank(&s->context,sizeof s->context)||s->yields)return UP_STATE;
        }else{
            if(s->state<UP_READY||s->state>UP_EXITED||spec_valid(&s->spec,c->safe_root)!=UP_OK||s->yields>1)return UP_STATE;
            if(s->context.pc<s->spec.code_va || s->context.pc-s->spec.code_va>=UP_PAGE ||
               s->context.sp<s->spec.stack_va.start || s->context.sp>s->spec.stack_va.end)return UP_CONTEXT;
            for(unsigned j=0;j<i;++j)if(c->slots[j].state!=UP_FREE &&
                (s->spec.root==c->slots[j].spec.root ||
                 overlap(s->spec.stack_va,c->slots[j].spec.stack_va) ||
                 s->spec.data_va==c->slots[j].spec.data_va))return UP_STATE;
            if(s->state==UP_RUNNING){++running;if(c->current!=(int)i)return UP_STATE;}
        }
    }
    if((c->current==-1 && running) || (c->current!=-1 && running!=1))return UP_STATE;
    if(c->dispatches!=c->phase+(c->current!=-1?1u:0u))return UP_STATE;
    return UP_OK;
}
void up_init(struct up_control *c,uint64_t safe_root)
{
    if(!pointer(c,sizeof *c,_Alignof(struct up_control)) || !safe_root ||
       (safe_root&(UP_PAGE-1u)))return;
    clear(c,sizeof *c);c->safe_root=safe_root;c->current=-1;
}
enum up_error up_create(struct up_control *c,const struct up_spec *spec,unsigned *id)
{
    enum up_error e=valid(c);if(e!=UP_OK)return e;
    if(!external(c,spec,sizeof *spec,_Alignof(struct up_spec)) ||
       !external(c,id,sizeof *id,_Alignof(unsigned)))return UP_ARGUMENT;
    if(c->current!=-1 || c->phase || c->released)return UP_STATE;
    if((e=spec_valid(spec,c->safe_root))!=UP_OK)return e;
    unsigned free_slot=UP_SLOTS;
    for(unsigned i=0;i<UP_SLOTS;++i){
        if(c->slots[i].state==UP_FREE){if(free_slot==UP_SLOTS)free_slot=i;}
        else if(c->slots[i].spec.root==spec->root ||
            overlap(c->slots[i].spec.stack_va,spec->stack_va) ||
            c->slots[i].spec.data_va==spec->data_va)return UP_OVERLAP;
    }
    if(free_slot==UP_SLOTS)return UP_FULL;
    struct up_slot *s=&c->slots[free_slot];s->spec=*spec;s->state=UP_READY;
    s->context.pc=spec->entry;s->context.sp=spec->stack_va.end;*id=free_slot;
    return UP_OK;
}
enum up_error up_go(struct up_control *c,unsigned *id)
{
    enum up_error e=valid(c);if(e!=UP_OK)return e;
    if(!external(c,id,sizeof *id,_Alignof(unsigned)))return UP_ARGUMENT;
    if(c->current!=-1)return UP_STATE;
    unsigned selected=UP_SLOTS;
    for(unsigned n=0;n<UP_SLOTS;++n){unsigned i=(c->next+n)%UP_SLOTS;if(c->slots[i].state==UP_READY){selected=i;break;}}
    if(selected==UP_SLOTS)return UP_EMPTY;
    if(c->phase>=4 || selected!=(c->phase&1u))return UP_STATE;
    if(c->dispatches==UINT_MAX)return UP_OVERFLOW;
    c->slots[selected].state=UP_RUNNING;c->current=(int)selected;
    c->next=(selected+1u)%UP_SLOTS;++c->dispatches;*id=selected;return UP_OK;
}
enum up_error up_accept(struct up_control *c,enum up_kind kind,uint64_t safe_root)
{
    enum up_error e=valid(c);if(e!=UP_OK)return e;
    if(c->current==-1 || c->phase>=4 || safe_root!=c->safe_root)return UP_STATE;
    enum up_kind expected=c->phase<2?UP_YIELD:c->phase==2?UP_FAULT:UP_EXIT;
    if(kind!=expected || (unsigned)c->current!=(c->phase&1u))return UP_OPERATION;
    struct up_slot *s=&c->slots[(unsigned)c->current];
    if(kind==UP_YIELD){if(s->yields)return UP_STATE;++s->yields;s->state=UP_READY;}
    else {if(s->yields!=1)return UP_STATE;s->state=kind==UP_FAULT?UP_FAULTED:UP_EXITED;}
    c->current=-1;++c->phase;return UP_OK;
}
enum up_error up_release(struct up_control *c,unsigned id,uint64_t safe_root)
{
    enum up_error e=valid(c);if(e!=UP_OK)return e;
    if(id>=UP_SLOTS)return UP_ARGUMENT;
    if(c->current!=-1 || c->phase!=4 || safe_root!=c->safe_root ||
       (c->slots[id].state!=UP_FAULTED && c->slots[id].state!=UP_EXITED))return UP_STATE;
    clear(&c->slots[id],sizeof c->slots[id]);++c->released;return UP_OK;
}
const char *up_error_name(enum up_error e)
{
    static const char *const names[]={"ok","argument","state","range","alignment","overlap","full","empty","overflow","platform","origin","operation","context","mapping"};
    return (unsigned)e<sizeof names/sizeof names[0]?names[e]:"unknown";
}
const char *up_state_name(enum up_state s)
{
    static const char *const names[]={"FREE","READY","RUNNING","FAULTED","EXITED"};
    return (unsigned)s<sizeof names/sizeof names[0]?names[s]:"unknown";
}
