#include <styleos/runtime_tick_860.h>
static bool span(struct boot_range r)
{
    return r.start&&r.start<r.end&&r.end<=(UINT64_C(1)<<47)&&
           !((r.start|r.end)&4095u);
}
static bool overlap(struct boot_range a,struct boot_range b)
{
    return a.start<b.end&&b.start<a.end;
}
enum ps_status dpr_tick_860(struct dpr_service *s,const struct rt860_bounds *b,
                          const struct rt860_tick *t,struct s847_raw *saved)
{
    if(!s||!s->started||!s->windows)return PS_STATE;
    if(!b||!t||!saved)return PS_ARGUMENT;
    if(!span(b->code)||!span(b->stack)||!span(b->ist)||
       overlap(b->code,b->stack)||overlap(b->code,b->ist)||overlap(b->stack,b->ist)||
       b->ist.end-b->ist.start!=4096||t->frame_va!=b->ist.end-16-sizeof(t->frame)||
       b->cs==b->ss||(b->cs&7u)!=3u||(b->ss&7u)!=3u||b->cs<8||b->ss<8||
       t->frame.vector!=t->vector||t->frame.error||
       t->frame.cs!=b->cs||t->frame.ss!=b->ss||
       t->frame.rip<b->code.start||t->frame.rip>=b->code.end||
       t->frame.rsp<=b->stack.start||t->frame.rsp>b->stack.end||
       (t->frame.rflags&0x202u)!=0x202u||(t->frame.rflags&~UINT64_C(0xad7)))return PS_EVIDENCE;
    enum ps_status status=ps_tick_860(&s->dispatcher,t);
    if(status!=PS_OK)return status;
    *saved=t->frame;
    /* Nonterminal event: do not reap windows or alter resident_bytes. */
    return PS_OK;
}
