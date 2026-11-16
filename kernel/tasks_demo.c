#include "styleos/arch.h"
#include "styleos/boot_pool.h"
#include "styleos/tasks.h"

_Alignas(4096) unsigned char styleos_tasks_stack_a[TASKS_STACK_BYTES];
_Alignas(4096) unsigned char styleos_tasks_stack_b[TASKS_STACK_BYTES];
_Alignas(4096) union tasks_demo_control styleos_tasks_control;
_Static_assert(sizeof styleos_tasks_control==TASKS_CONTROL_BYTES,"control extent");
_Static_assert(sizeof(struct tasks_demo)<=TASKS_CONTROL_BYTES,"control capacity");

static struct tasks_demo *demo(void) { return &styleos_tasks_control.demo; }
static _Noreturn void fail(const char *why)
{
    arch_puts("STYLEOS TASKS ERROR: "); arch_puts(why); arch_putc('\n'); arch_halt();
}
static void check(enum tasks_error e)
{
    if (e!=TASKS_OK) fail(tasks_error_name(e));
}
static void hex(uint64_t v)
{
    arch_puts("0x");
    for (int n=60;n>=0;n-=4) arch_putc("0123456789abcdef"[(v>>n)&15u]);
}
static void field(const char *name,uint64_t v) { arch_puts(name);hex(v); }
static void identity(unsigned id) { arch_putc(id==0?'A':'B'); }
static void named(const char *kind,unsigned id)
{
    arch_puts("STYLEOS TASKS ");arch_puts(kind);arch_puts(" id=");identity(id);
}
static struct boot_range extent(const void *p,size_t bytes)
{
    struct boot_range r;
    if (bm_extent((uintptr_t)p,bytes,&r)) fail("extent overflow");
    return r;
}
static int inside(struct boot_range a,struct boot_range b)
{
    return a.start<a.end && b.start<=a.start && a.end<=b.end;
}
static int overlaps(struct boot_range a,struct boot_range b)
{
    return a.start<b.end && b.start<a.end;
}
static int point(struct boot_range r,uint64_t address,uint64_t bytes)
{
    return bytes && address>=r.start && address<r.end && bytes<=r.end-address;
}
static struct boot_range stack(unsigned id)
{
    return extent(id==0?styleos_tasks_stack_a:styleos_tasks_stack_b,TASKS_STACK_BYTES);
}
static struct boot_range usable(unsigned id)
{
    struct boot_range r=stack(id);r.start+=TASKS_GUARD_BYTES;r.end-=TASKS_GUARD_BYTES;return r;
}
static void range_line(const char *name,struct boot_range r)
{
    arch_puts("STYLEOS TASKS ");arch_puts(name);arch_putc(' ');
    hex(r.start);arch_putc(' ');hex(r.end);arch_putc('\n');
}
static void state_line(const char *label,const struct tasks_arch_state *s)
{
    arch_puts(label);field(" mode=",s->mode);field(" root=",s->root);
    field(" control0=",s->control0);field(" control1=",s->control1);
    field(" flags=",s->flags);field(" cs=",s->cs);
    field(" idt_base=",s->idt_base);field(" idt_bytes=",s->idt_bytes);
    field(" gdt_base=",s->gdt_base);field(" gdt_bytes=",s->gdt_bytes);
    field(" pat=",s->pat);field(" pa_bits=",s->pa_bits);field(" gdt_pages=",s->gdt_pages);
    field(" gdt_pa0=",s->gdt_pa0);field(" gdt_pa1=",s->gdt_pa1);arch_putc('\n');
}
static void admit(const struct boot_memory_evidence *e,struct tasks_arch_state *before)
{
    struct boot_range pool=boot_pool_storage_range(),meta=boot_pool_metadata_range();
    if (boot_pool_check_layout(pool.start,pool.end-pool.start,meta.start,
                              meta.end-meta.start,e)!=BP_OK) fail("memory evidence");
    const struct boot_range own[]={stack(0),stack(1),extent(&styleos_tasks_control,sizeof styleos_tasks_control)};
    const struct boot_range excluded[]={e->live.stack,e->live.handoff,e->live.source,
                                        e->workspace,pool,meta};
    for (size_t n=0;n<3;++n) {
        if ((own[n].start|own[n].end)&4095u || !inside(own[n],e->live.image)) fail("owned range");
        for (size_t k=0;k<sizeof excluded/sizeof excluded[0];++k)
            if (overlaps(own[n],excluded[k])) fail("owned overlap");
        for (size_t k=0;k<n;++k) if (overlaps(own[n],own[k])) fail("stack overlap");
    }
    check(tasks_arch_observe(e,before));
    struct boot_range idt;
    if (bm_extent(before->idt_base,before->idt_bytes,&idt) || !inside(idt,e->live.image))
        fail("exception metadata");
    for (size_t n=0;n<3;++n) if (overlaps(own[n],idt)) fail("exception overlap");
}
static unsigned reg_count(void) { return STYLEOS_EXPECT_ARCH==1?8u:11u; }
static uint64_t full_mask(void) { return (UINT64_C(1)<<reg_count())-1u; }
static uint64_t tag(unsigned id,unsigned seq) { return ((uint64_t)(id+1u)<<8)|seq; }
static uint64_t pattern(unsigned n,uint64_t t) { return TASKS_REG_BASE+((uint64_t)n<<32)+t; }
static unsigned running(void)
{
    int id=demo()->control.current;
    if (id<0 || id>=(int)TASKS_MAX || demo()->control.slots[id].state!=TASKS_RUNNING)
        fail("current task");
    return (unsigned)id;
}
static uint64_t load64(const unsigned char *p)
{
    uint64_t v=0;for (unsigned n=0;n<8;++n) v|=(uint64_t)p[n]<<(n*8u);return v;
}
#if STYLEOS_EXPECT_ARCH == 1
static void store64(unsigned char *p,uint64_t v)
{
    for (unsigned n=0;n<8;++n) p[n]=(unsigned char)(v>>(n*8u));
}
#endif
static void guards(int print)
{
    for (unsigned id=0;id<TASKS_MAX;++id) {
        const unsigned char *p=id==0?styleos_tasks_stack_a:styleos_tasks_stack_b;
        for (unsigned n=0;n<TASKS_GUARD_BYTES;++n)
            if (p[n]!=0x6bu || p[TASKS_STACK_BYTES-TASKS_GUARD_BYTES+n]!=0xa5u)
                fail("stack canary");
        if (print) {
            named("CANARY",id);field(" low0=",load64(p));field(" low1=",load64(p+8));
            field(" high0=",load64(p+TASKS_STACK_BYTES-16));
            field(" high1=",load64(p+TASKS_STACK_BYTES-8));arch_putc('\n');
        }
    }
}
static void initial_context(unsigned id)
{
    unsigned char *p=id==0?styleos_tasks_stack_a:styleos_tasks_stack_b;
    for (unsigned n=0;n<TASKS_GUARD_BYTES;++n) {
        p[n]=0x6bu;p[TASKS_STACK_BYTES-TASKS_GUARD_BYTES+n]=0xa5u;
    }
    struct tasks_context *c=&demo()->control.slots[id].context;
    *c=(struct tasks_context){0};
    uint64_t top=usable(id).end;
#if STYLEOS_EXPECT_ARCH == 1
    c->sp=top-40u;
    store64(p+(size_t)(c->sp-stack(id).start),(uint64_t)(uintptr_t)styleos_tasks_trap_return);
#else
    c->sp=top;c->gpr[11]=(uint64_t)(uintptr_t)styleos_tasks_trap_return;
#endif
    c->pc=(uint64_t)(uintptr_t)styleos_tasks_bootstrap;
}
static uint64_t yield_task(void)
{
    struct tasks_demo *d=demo();unsigned id=running();
    struct tasks_slot *slot=&d->control.slots[id];
    if (slot->yields>=2) fail("yield count");
    check(tasks_mark_yield(&d->control));
    uint64_t t=tag(id,slot->yields);
    named("YIELD",id);field(" seq=",slot->yields);field(" tag=",t);arch_putc('\n');
    uint64_t mask=tasks_arch_probe(&slot->context,&d->control.dispatcher,t);
    if (running()!=id || mask!=full_mask()) fail("restored registers");
    d->masks[id]=mask;return mask;
}
static void event(unsigned id,unsigned phase,volatile uint64_t *cookie,uint64_t mask)
{
    struct tasks_demo *d=demo();uint64_t address=(uint64_t)(uintptr_t)cookie;
    if (running()!=id || phase>=3 || d->events[id]!=phase ||
        *cookie!=(id==0?TASKS_COOKIE_A:TASKS_COOKIE_B) ||
        !point(usable(id),address,sizeof *cookie) ||
        (phase && (address!=d->locals[id] || mask!=full_mask())) || (!phase && mask))
        fail("task local or phase");
    if (!phase) d->locals[id]=address;
    ++d->events[id];
    named("EVENT",id);arch_puts(" kind=");
    arch_puts(phase==0?"START":phase==1?"RESUME":"EXIT");
    field(" local=",address);field(" value=",*cookie);field(" regs=",mask);arch_putc('\n');
}
static __attribute__((noinline)) void body(unsigned id)
{
    volatile uint64_t cookie=id==0?TASKS_COOKIE_A:TASKS_COOKIE_B;
    event(id,0,&cookie,0);
    event(id,1,&cookie,yield_task());
    event(id,2,&cookie,yield_task());
}
__attribute__((noinline)) void styleos_tasks_a(void) { body(0); }
__attribute__((noinline)) void styleos_tasks_b(void) { body(1); }

_Noreturn void tasks_completed_resumed(void) { fail("completed task resumed"); }
_Noreturn void styleos_tasks_bootstrap(void)
{
    struct tasks_demo *d=demo();unsigned id=running();
    if (d->events[id] || d->returns[id]) fail("bootstrap reentry");
    d->control.slots[id].entry();
    if (running()!=id || d->events[id]!=3 || d->control.slots[id].yields!=2 || d->returns[id])
        fail("task return sequence");
    ++d->returns[id];named("RETURN",id);arch_putc('\n');
    check(tasks_mark_done(&d->control));
    tasks_arch_finish(&d->control.slots[id].context,&d->control.dispatcher);
}
static void saved(unsigned id)
{
    struct tasks_demo *d=demo();struct tasks_slot *s=&d->control.slots[id];
    if (d->control.current!=-1 || !point(usable(id),s->context.sp,1) || (s->context.sp&15u))
        fail("saved task stack");
    int ready=s->state==TASKS_READY;
    if ((!ready && s->state!=TASKS_DONE) ||
        s->context.pc!=(uint64_t)(uintptr_t)(ready?styleos_tasks_probe_resume:styleos_tasks_exit_resume))
        fail("saved task continuation");
    uint64_t mask=0;
    if (ready) for (unsigned n=0;n<reg_count();++n)
        if (s->context.gpr[n]==pattern(n,tag(id,s->yields))) mask|=UINT64_C(1)<<n;
    if (ready && mask!=full_mask()) fail("saved registers");
#if STYLEOS_EXPECT_ARCH == 2
    if (s->context.gpr[11]!=s->context.pc) fail("saved link register");
#else
    for (unsigned n=8;n<12;++n) if (s->context.gpr[n]) fail("unused context field");
#endif
    named("SAVED",id);arch_puts(" state=");arch_puts(tasks_state_name(s->state));
    field(" sp=",s->context.sp);field(" pc=",s->context.pc);
    field(" seq=",s->yields);field(" mask=",mask);arch_putc('\n');
    if (ready) for (unsigned n=0;n<reg_count();++n) {
        named("REG",id);field(" seq=",s->yields);field(" index=",n);
        field(" value=",s->context.gpr[n]);arch_putc('\n');
    }
}

void tasks_demo_run(const struct boot_memory_evidence *e)
{
    struct tasks_arch_state before;
    admit(e,&before);
    struct tasks_demo *d=demo();*d=(struct tasks_demo){0};
    d->evidence=e;d->before=before;tasks_init(&d->control);
    range_line("STACK id=A",stack(0));range_line("STACK id=B",stack(1));
    range_line("CONTROL",extent(&styleos_tasks_control,sizeof styleos_tasks_control));
    range_line("MAP_WORKSPACE",e->workspace);range_line("DISPATCH_STACK",e->live.stack);
    state_line("STYLEOS TASKS STATE",&d->before);
    for (unsigned n=0;n<TASKS_MAX;++n) {
        unsigned id=TASKS_MAX;
        check(tasks_create(&d->control,n==0?styleos_tasks_a:styleos_tasks_b,stack(n),&id));
        if (id!=n) fail("creation order");
        initial_context(id);named("CREATE",id);arch_puts(" state=READY");
        field(" sp=",d->control.slots[id].context.sp);field(" pc=",d->control.slots[id].context.pc);
        field(" entry=",(uint64_t)(uintptr_t)d->control.slots[id].entry);arch_putc('\n');
    }
    volatile uint64_t dispatcher_cookie=TASKS_DISPATCH_COOKIE;
    if (!point(e->live.stack,(uintptr_t)&dispatcher_cookie,sizeof dispatcher_cookie))
        fail("dispatcher local");
    for (unsigned turn=0;turn<6;++turn) {
        unsigned id=TASKS_MAX;check(tasks_go(&d->control,&id));
        if (id!=turn%TASKS_MAX) fail("dispatch order");
        named("GO",id);field(" count=",d->control.dispatches);arch_putc('\n');
        tasks_arch_dispatch(&d->control.dispatcher,&d->control.slots[id].context);
        saved(id);guards(0);
        if (dispatcher_cookie!=TASKS_DISPATCH_COOKIE ||
            !point(e->live.stack,d->control.dispatcher.sp,1) || (d->control.dispatcher.sp&15u) ||
            d->control.dispatcher.pc!=(uint64_t)(uintptr_t)styleos_tasks_dispatch_resume)
            fail("dispatcher context");
        arch_puts("STYLEOS TASKS DISPATCH");field(" local=",(uintptr_t)&dispatcher_cookie);
        field(" value=",dispatcher_cookie);field(" sp=",d->control.dispatcher.sp);
        field(" pc=",d->control.dispatcher.pc);arch_putc('\n');
    }
    unsigned unused=TASKS_MAX;
    if (tasks_go(&d->control,&unused)!=TASKS_EMPTY || unused!=TASKS_MAX ||
        d->control.completed!=2 || d->control.dispatches!=6) fail("dispatcher completion");
    for (unsigned id=0;id<TASKS_MAX;++id)
        if (d->events[id]!=3 || d->returns[id]!=1 || d->control.slots[id].state!=TASKS_DONE ||
            d->control.slots[id].yields!=2 || d->masks[id]!=full_mask()) fail("task lifecycle");
    arch_puts("STYLEOS TASKS EMPTY");field(" completed=",d->control.completed);
    field(" dispatches=",d->control.dispatches);arch_putc('\n');
    for (unsigned id=0;id<TASKS_MAX;++id) {
        check(tasks_release(&d->control,id));named("RELEASE",id);arch_puts(" state=FREE\n");
    }
    if (tasks_go(&d->control,&unused)!=TASKS_EMPTY) fail("released task restarted");
    guards(1);arch_puts("STYLEOS TASKS CANARY PASS\n");
    check(tasks_arch_observe(e,&d->after));
    if (!tasks_arch_unchanged(&d->before,&d->after)) fail("architectural state changed");
    state_line("STYLEOS TASKS STATE AFTER",&d->after);
    arch_puts("STYLEOS TASKS PASS\n");
}
