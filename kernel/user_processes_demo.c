#include "styleos/arch.h"
#include "styleos/boot_pool.h"
#include "styleos/user_processes.h"
#include "styleos/process_scheduler.h"
#if defined(STYLEOS_TEST_NATIVE_TEA_SESSION) || defined(STYLEOS_TEST_USER_IPC_773)
#include "styleos/native_session_hw_770.h"
static struct nsh_stage observed_sessions;
#endif
#ifdef STYLEOS_TEST_USER_IPC_773
#include "styleos/user_ipc_773.h"
struct ipc773_broker observed_ipc_773;
#endif
#ifdef STYLEOS_TEST_NATIVE_TEA
#include "styleos/tea_768_demo.h"
#endif

_Alignas(4096) uint64_t styleos_user_tables[UP_ROOTS][UP_TABLE_PAGES][512];
_Alignas(4096) union up_demo_control styleos_user_control;
_Alignas(4096) unsigned char styleos_user_entry_stack[UP_ENTRY_STACK_BYTES];
_Alignas(4096) unsigned char styleos_user_stack_a[UP_PAGE],styleos_user_stack_b[UP_PAGE];
_Alignas(4096) uint64_t styleos_user_data_a[UP_PAGE/8],styleos_user_data_b[UP_PAGE/8];
_Static_assert(sizeof styleos_user_tables==393216,"complete table reserve");
_Static_assert(sizeof styleos_user_control==UP_CONTROL_BYTES,"complete control");

/* Test-image-only scheduler state, isolated from desktop/ordinary profiles.
 * Entries come from actual prepared CPL3/EL0 contexts and verified traps. */
static struct pr_table observed_registry;
static struct ps_dispatcher observed_scheduler;
static uint32_t observed_pid[UP_SLOTS];
static uint64_t observed_handle[UP_SLOTS];
static void pscheck(enum ps_status s)
{ if (s != PS_OK) up_fail("process scheduler bridge"); }


_Noreturn void up_fail(const char *why)
{
    arch_puts("STYLEOS USER ERROR: ");arch_puts(why);arch_putc('\n');arch_halt();
}
void up_hex(uint64_t v)
{
    arch_puts("0x");for(int n=60;n>=0;n-=4)arch_putc("0123456789abcdef"[(v>>n)&15u]);
}
void up_field(const char *name,uint64_t value){arch_puts(name);up_hex(value);}
void up_range_line(const char *name,struct boot_range r)
{
    arch_puts("STYLEOS USER ");arch_puts(name);arch_putc(' ');up_hex(r.start);
    arch_putc(' ');up_hex(r.end);arch_putc('\n');
}
static void check(enum up_error e){if(e!=UP_OK)up_fail(up_error_name(e));}
static void id_text(unsigned id){arch_putc(id==0?'A':'B');}
static void named(const char *name,unsigned id)
{
    arch_puts("STYLEOS USER ");arch_puts(name);arch_puts(" id=");id_text(id);
}
static struct boot_range extent(const void *p,size_t bytes)
{
    struct boot_range r;if(bm_extent((uintptr_t)p,bytes,&r)!=BM_OK)up_fail("extent");return r;
}
static int inside(struct boot_range outer,struct boot_range inner)
{
    return inner.start<inner.end && outer.start<=inner.start && inner.end<=outer.end;
}
static int overlap(struct boot_range a,struct boot_range b)
{
    return a.start<b.end && b.start<a.end;
}
static int point(struct boot_range r,uint64_t address,uint64_t bytes)
{
    return bytes && address>=r.start && address<r.end && bytes<=r.end-address;
}
static uint64_t current_sp(void)
{
    uint64_t sp;
#if STYLEOS_EXPECT_ARCH == 1
    __asm__ volatile("mov %%rsp,%0":"=r"(sp));
#else
    __asm__ volatile("mov %0,sp":"=r"(sp));
#endif
    return sp;
}
static struct boot_range stack_va(unsigned id)
{
    uint64_t base=id?UP_STACK_B_VA:UP_STACK_A_VA;
    return (struct boot_range){base,base+UP_PAGE};
}
static void admit_common(const struct boot_memory_evidence *e)
{
    struct boot_range pool=boot_pool_storage_range(),metadata=boot_pool_metadata_range();
    if(boot_pool_check_layout(pool.start,pool.end-pool.start,metadata.start,
                             metadata.end-metadata.start,e)!=BP_OK)up_fail("evidence");
    const struct boot_range own[]={
        extent(styleos_user_tables,sizeof styleos_user_tables),
        extent(&styleos_user_control,sizeof styleos_user_control),
        extent(styleos_user_entry_stack,sizeof styleos_user_entry_stack),
        extent(styleos_user_stack_a,sizeof styleos_user_stack_a),
        extent(styleos_user_stack_b,sizeof styleos_user_stack_b),
        extent(styleos_user_data_a,sizeof styleos_user_data_a),
        extent(styleos_user_data_b,sizeof styleos_user_data_b),
        extent(styleos_user_code_a,UP_PAGE),extent(styleos_user_code_b,UP_PAGE)
#ifdef STYLEOS_TEST_NATIVE_TEA
        ,extent(styleos_tea_768_code_b,NTE_PAGE)
#ifdef STYLEOS_TEST_NATIVE_TEA_DISK
        ,extent(styleos_tea_769_package,NTP_MAX_PACKAGE)
        ,extent(styleos_tea_769_sector,NTE_PAGE)
#endif
#endif
    };
    const struct boot_range excluded[]={e->live.stack,e->live.handoff,e->live.source,
                                        e->workspace,pool,metadata};
    for(size_t n=0;n<sizeof own/sizeof own[0];++n){
        if((own[n].start|own[n].end)&(UP_PAGE-1u) || !inside(e->live.image,own[n]))up_fail("owned interval");
        for(size_t k=0;k<n;++k)if(overlap(own[n],own[k]))up_fail("owned overlap");
        for(size_t k=0;k<sizeof excluded/sizeof excluded[0];++k)
            if(overlap(own[n],excluded[k]))up_fail("live overlap");
    }
    /* No new buffer is written here. The architecture must establish the old
     * mapping/physical-access proof before its first initialization write. */
}
static void objects(const struct boot_memory_evidence *e)
{
    up_range_line("OBJECT name=tables",extent(styleos_user_tables,sizeof styleos_user_tables));
    up_range_line("OBJECT name=control",extent(&styleos_user_control,sizeof styleos_user_control));
    up_range_line("OBJECT name=entry_stack",extent(styleos_user_entry_stack,sizeof styleos_user_entry_stack));
    up_range_line("OBJECT name=stack_a",extent(styleos_user_stack_a,sizeof styleos_user_stack_a));
    up_range_line("OBJECT name=stack_b",extent(styleos_user_stack_b,sizeof styleos_user_stack_b));
    up_range_line("OBJECT name=data_a",extent(styleos_user_data_a,sizeof styleos_user_data_a));
    up_range_line("OBJECT name=data_b",extent(styleos_user_data_b,sizeof styleos_user_data_b));
    up_range_line("OBJECT name=code_a",extent(styleos_user_code_a,UP_PAGE));
    up_range_line("OBJECT name=code_b",extent(styleos_user_code_b,UP_PAGE));
#ifdef STYLEOS_TEST_NATIVE_TEA
    up_range_line("OBJECT name=tea_code_b",extent(styleos_tea_768_code_b,NTE_PAGE));
#ifdef STYLEOS_TEST_NATIVE_TEA_DISK
    up_range_line("OBJECT name=tea_disk_buffer",extent(styleos_tea_769_package,NTP_MAX_PACKAGE));
    up_range_line("OBJECT name=tea_sector_buffer",extent(styleos_tea_769_sector,NTE_PAGE));
#endif
#endif
    up_range_line("OBJECT name=stack",e->live.stack);
    up_range_line("OBJECT name=diagnostic",e->workspace);
    up_range_line("OBJECT name=pool",boot_pool_storage_range());
    up_range_line("OBJECT name=pool_metadata",boot_pool_metadata_range());
#if STYLEOS_EXPECT_ARCH == 1
    up_range_line("OBJECT name=handoff",e->live.handoff);
#endif
}
static const char *root_name(unsigned r){return r==0?"SAFE":r==1?"A":"B";}
static void tables(const struct up_platform *p)
{
    for(unsigned r=0;r<UP_ROOTS;++r){
        if(p->roots[r]!=(uint64_t)(uintptr_t)&styleos_user_tables[r][0][0] ||
           !p->table_pages[r] || p->table_pages[r]>UP_TABLE_PAGES)up_fail("table root");
        arch_puts("STYLEOS USER ROOT id=");arch_puts(root_name(r));
        up_field(" pa=",p->roots[r]);up_field(" pages=",p->table_pages[r]);arch_putc('\n');
        uint64_t entries=0;
        /* Scan ALL pages, including declared unused ones. Thus omitted rows
         * mean observed zero, not an uninspected alternative mapping. */
        for(unsigned t=0;t<UP_TABLE_PAGES;++t)for(unsigned i=0;i<512;++i){
            uint64_t entry=((volatile uint64_t (*)[512])styleos_user_tables[r])[t][i];
            if(!entry)continue;
            if(t>=p->table_pages[r])up_fail("unused table not zero");
            ++entries;arch_puts("STYLEOS USER TABLE id=");arch_puts(root_name(r));
            up_field(" table=",p->roots[r]+(uint64_t)t*UP_PAGE);up_field(" index=",i);up_field(" entry=",entry);arch_putc('\n');
        }
        arch_puts("STYLEOS USER TABLES END id=");arch_puts(root_name(r));
        up_field(" entries=",entries);arch_putc('\n');
    }
}
static void machine_line(const char *phase,const struct up_machine *m)
{
    arch_puts("STYLEOS USER MACHINE phase=");arch_puts(phase);
    up_field(" root=",m->root);up_field(" mode=",m->mode);
    up_field(" control0=",m->control0);up_field(" control1=",m->control1);
    up_field(" control2=",m->control2);up_field(" control3=",m->control3);
    up_field(" vectors=",m->vectors);up_field(" gdt=",m->gdt);up_field(" tss=",m->tss);arch_putc('\n');
}
static int machine_same(const struct up_machine *a,const struct up_machine *b)
{
    return a->root==b->root && a->mode==b->mode && a->control0==b->control0 &&
        a->control1==b->control1 && a->control2==b->control2 && a->control3==b->control3 &&
        a->vectors==b->vectors && a->gdt==b->gdt && a->tss==b->tss;
}
static const char *kind_name(enum up_kind k)
{
    return k==UP_YIELD?"YIELD":k==UP_FAULT?"FAULT":k==UP_EXIT?"EXIT":"INVALID";
}
static uint64_t private_data(unsigned id)
{
    return id?*(volatile uint64_t *)styleos_user_data_b:*(volatile uint64_t *)styleos_user_data_a;
}
static uint64_t stack_cookie(unsigned id,const struct up_context *c)
{
    struct boot_range r=stack_va(id);
    if(!point(r,c->sp,8))up_fail("user SP");
    /* Trusted translation inside this explicitly known static page only;
     * no user pointer is accepted as an operation argument. */
    unsigned char *base=id?styleos_user_stack_b:styleos_user_stack_a;
    return *(volatile uint64_t *)(void *)(base+(size_t)(c->sp-r.start));
}
static void validate_frame(unsigned turn,unsigned id,const struct up_context *c,
                           const struct up_event *ev,const struct up_platform *p)
{
    enum up_kind expected=turn<2?UP_YIELD:turn==2?UP_FAULT:UP_EXIT;
    uint64_t cookie=id?UP_DATA_BEFORE:UINT64_C(0xa11ce55a12345678);
    if(id!=(turn&1u)||ev->kind!=expected||ev->active_root!=p->roots[id+1] ||
       c->sp!=stack_va(id).end-16 || stack_cookie(id,c)!=cookie)up_fail("hardware context");
    uint64_t expected_pc=turn<2?p->resume_pc[id]:turn==2?p->fault_pc[0]:p->exit_pc[1]+(STYLEOS_EXPECT_ARCH==1?2u:4u);
    if(c->pc!=expected_pc || !point((struct boot_range){UP_CODE_VA,UP_CODE_VA+UP_PAGE},c->pc,1))up_fail("user PC");
    struct boot_range entry=extent(styleos_user_entry_stack,sizeof styleos_user_entry_stack);
#if STYLEOS_EXPECT_ARCH == 1
    if(!point(entry,ev->kernel_sp,176) || c->cs!=0x23 || c->ss!=0x1b ||
       !(c->status&2u) || (c->status&(UINT64_C(0x200)|UINT64_C(0x400)|UINT64_C(0x3000)|UINT64_C(0x4000)|UINT64_C(0x20000))) ||
       c->gpr[1]!=(id?UINT64_C(0xb22c):UINT64_C(0xa11c)) || c->gpr[11]!=cookie)up_fail("CPL3 frame");
    for(unsigned n=15;n<31;++n)if(c->gpr[n])up_fail("x86 context tail");
    if(expected==UP_FAULT){if(ev->vector!=14 || ev->syndrome!=4 || ev->fault_va!=UP_DATA_B_VA)up_fail("user page fault");}
    else if(ev->vector!=128 || ev->syndrome || ev->fault_va ||
#ifdef STYLEOS_TEST_USER_SYSCALL
        c->gpr[0]!=(turn==0?3u:expected==UP_YIELD?1u:2u))up_fail("user operation");
#else
        c->gpr[0]!=(expected==UP_YIELD?1u:2u))up_fail("user operation");
#endif
#ifdef STYLEOS_TEST_USER_SYSCALL
    if(turn==0 && (c->gpr[2]!=8u ||
#ifdef STYLEOS_TEST_USER_SYSCALL_DENY
       c->gpr[3]!=UP_DATA_B_VA))up_fail("772 CPL3 request registers");
#else
       c->gpr[3]!=UP_STACK_A_VA+UP_PAGE-16u))up_fail("772 CPL3 request registers");
#endif
#endif
#else
    if(!point(entry,ev->kernel_sp,320) || (c->status&UINT64_C(0x3df))!=UINT64_C(0x3c0) ||
       c->cs || c->ss || c->gpr[19]!=(id?UINT64_C(0xb22c):UINT64_C(0xa11c)) || c->gpr[20]!=cookie || ev->vector!=8)up_fail("EL0 frame");
    if(expected==UP_FAULT){if(ev->syndrome!=UINT64_C(0x92000007) || ev->fault_va!=UP_DATA_B_VA)up_fail("EL0 abort");}
    else if(ev->syndrome!=(expected==UP_YIELD?UINT64_C(0x56000001):UINT64_C(0x56000002)) || ev->fault_va)up_fail("user SVC");
#endif
    if(id && c->gpr[9]!=UINT64_C(0x55aa001122334455))up_fail("volatile user register");
    if(private_data(0)!=0 || private_data(1)!=(turn==0?0:turn==3?UP_DATA_AFTER:UP_DATA_BEFORE))up_fail("private user data");
}
static void frame_line(unsigned id,const struct up_context *c,const struct up_event *ev)
{
    named("FRAME",id);arch_puts(" kind=");arch_puts(kind_name(ev->kind));
    up_field(" vector=",ev->vector);up_field(" syndrome=",ev->syndrome);
    up_field(" address=",ev->fault_va);up_field(" root=",ev->active_root);
    up_field(" kernel_sp=",ev->kernel_sp);up_field(" pc=",c->pc);up_field(" sp=",c->sp);
    up_field(" status=",c->status);up_field(" cs=",c->cs);up_field(" ss=",c->ss);arch_putc('\n');
    for(unsigned n=0;n<31;++n){named("REG",id);up_field(" index=",n);up_field(" value=",c->gpr[n]);arch_putc('\n');}
}
void up_demo_run(const struct boot_memory_evidence *e)
{
    admit_common(e);
    struct up_platform local_platform={0};
    /* prepare performs read-only inherited-platform proof before writing new
     * buffers, then installs only the bounded SAFE mapping. Its output lives
     * on the old known stack until admission has completed. */
    check(up_arch_prepare(e,&local_platform));
    struct up_demo *d=&styleos_user_control.demo;*d=(struct up_demo){0};
    d->platform=local_platform;d->evidence=e;
    up_init(&d->control,d->platform.roots[UP_ROOT_SAFE]);
    pr_init(&observed_registry);
    pscheck(ps_init(&observed_scheduler,&observed_registry,d->platform.roots[UP_ROOT_SAFE]));
#if defined(STYLEOS_TEST_NATIVE_TEA_SESSION) || defined(STYLEOS_TEST_USER_IPC_773)
    if(!nsh_init(&observed_sessions,&observed_registry))up_fail("session initialize");
#endif
#ifdef STYLEOS_TEST_USER_IPC_773
    if(!ipc773_start(&observed_ipc_773,&observed_sessions))up_fail("773 bridge init");
#endif
    objects(e);tables(&d->platform);
    struct up_machine initial;check(up_arch_observe(&initial));
    if(initial.root!=d->platform.roots[UP_ROOT_SAFE])up_fail("safe activation");
    d->platform.safe=initial;machine_line("SAFE",&initial);
    for(unsigned id=0;id<UP_SLOTS;++id){
        struct up_spec spec={d->platform.roots[id+1],d->platform.entry[id],UP_CODE_VA,
                            id?UP_DATA_B_VA:UP_DATA_A_VA,stack_va(id)};
        unsigned created=UP_SLOTS;check(up_create(&d->control,&spec,&created));if(created!=id)up_fail("creation order");
        observed_handle[id]=(uint64_t)(uintptr_t)&d->control.slots[id];
        pscheck(ps_register(&observed_scheduler,id,observed_handle[id],spec.root,
                            0,id?"EL0/CPL3 User B":"EL0/CPL3 User A",
                            &observed_pid[id]));
#if defined(STYLEOS_TEST_NATIVE_TEA_SESSION) || defined(STYLEOS_TEST_USER_IPC_773)
        if(!nsh_attach(&observed_sessions,id,observed_pid[id],
                       observed_handle[id],spec.root))up_fail("session attach");
#endif
#if STYLEOS_EXPECT_ARCH == 1
        d->control.slots[id].context.status=2;d->control.slots[id].context.cs=0x23;d->control.slots[id].context.ss=0x1b;
#else
        d->control.slots[id].context.status=UINT64_C(0x3c0);
#endif
    }
    volatile uint64_t dispatcher_cookie=UP_DISPATCH_COOKIE;
    uint64_t dispatcher_sp=current_sp();
    if(!point(e->live.stack,(uintptr_t)&dispatcher_cookie,8) || !point(e->live.stack,dispatcher_sp,1))up_fail("dispatcher stack");
    if(observed_pid[0]==0 || observed_pid[1]<=observed_pid[0] ||
       pr_snapshot(&observed_registry,0,0,false)!=2)up_fail("PID admission");
#ifdef STYLEOS_TEST_NATIVE_TEA
    /* Report only after both real scheduler admissions have succeeded.
     * Previously this guard ran before ps_register() and halted the guest
     * before the first CPL3 transition with "STYLEOS USER ERROR: tea PID". */
    tea_768_report(observed_pid[1]);
#ifdef STYLEOS_TEST_NATIVE_TEA_SESSION
    if(!nsh_review(&observed_sessions))up_fail("session review");
    arch_puts("STYLEOS TEA770 SESSION REGISTERED real_pids=2 separated_roots=2 windows=2\n");
#endif
#endif
#ifdef STYLEOS_TEST_USER_IPC_773
    if(!nsh_review(&observed_sessions) || !ipc773_arm(&observed_ipc_773))
        up_fail("773 window arm");
    arch_puts("STYLEOS IPC773 WINDOW PORT ARMED\n");
#endif
    for(unsigned turn=0;turn<4;++turn){
        unsigned chosen=UP_SLOTS;uint32_t pid=0;
        pscheck(ps_next(&observed_scheduler,&chosen,&pid));
        unsigned id=UP_SLOTS;check(up_go(&d->control,&id));
        if(chosen!=id || pid!=observed_pid[id] ||
           pr_find(&observed_registry,pid)->state!=PR_RUNNING)up_fail("schedule selection");
        struct up_slot *slot=&d->control.slots[id];
        if(id!=(turn&1u))up_fail("dispatch order");
        if(turn==2 && slot->context.pc!=d->platform.resume_pc[0])up_fail("A continuation");
        if(turn==3 && (slot->context.pc!=d->before_b.pc || slot->context.sp!=d->before_b.sp ||
                       private_data(1)!=d->b_before || d->control.slots[0].state!=UP_FAULTED))up_fail("B continuation after fault");
        named("GO",id);up_field(" count=",d->control.dispatches);up_field(" root=",slot->spec.root);
        up_field(" pc=",slot->context.pc);up_field(" sp=",slot->context.sp);arch_putc('\n');
        struct up_event event={0};check(up_arch_run(id,&slot->context,&event));
        struct up_machine now;check(up_arch_observe(&now));
        if(!machine_same(&initial,&now) || dispatcher_cookie!=UP_DISPATCH_COOKIE ||
           current_sp()!=dispatcher_sp)up_fail("dispatcher return");
        validate_frame(turn,id,&slot->context,&event,&d->platform);
#ifdef STYLEOS_TEST_USER_SYSCALL
        if(turn==0){
            extern unsigned uc772_observed_state(void);
            extern uint64_t uc772_observed_cookie(void), uc772_observed_source(void);
#ifdef STYLEOS_TEST_USER_SYSCALL_DENY
            if(uc772_observed_state()!=2 || uc772_observed_cookie()!=0 ||
               uc772_observed_source()!=UP_DATA_B_VA)up_fail("772 copied a foreign page");
            arch_puts("STYLEOS UC772 HARDWARE DENIED foreign_user_page root=");
#else
            if(uc772_observed_state()!=1 ||
               uc772_observed_cookie()!=UINT64_C(0xa11ce55a12345678) ||
               uc772_observed_source()!=UP_STACK_A_VA+UP_PAGE-16u)
                up_fail("772 CPL3 payload evidence");
            arch_puts("STYLEOS UC772 HARDWARE COPIED pid=");
            up_hex(observed_pid[0]);arch_puts(" root=");
#endif
            up_hex(event.active_root);arch_puts(" source=");
            up_hex(uc772_observed_source());arch_puts(" len=0x0000000000000008");
            arch_putc('\n');
        }
#endif
#ifdef STYLEOS_TEST_USER_IPC_773
        if(turn==0){
            extern unsigned ipc773_denial_count(void);
            const unsigned refused=ipc773_denial_count();
#ifdef STYLEOS_TEST_USER_IPC_773_DENY
            if(refused!=1u)up_fail("773 foreign reply not refused");
#else
            if(refused!=0u)up_fail("773 unexpected denied reply");
#endif
            if(observed_ipc_773.returns!=1u ||
               nwc_port_count(&observed_ipc_773.channel)!=1 ||
               !observed_ipc_773.channel.ports[0].window_id ||
               observed_ipc_773.channel.ports[0].length!=0)
                up_fail("773 returned window event");
            arch_puts("STYLEOS IPC773 HARDWARE IRET COPY PASS pid=");
            up_hex(observed_pid[0]);up_field(" root=",event.active_root);
            up_field(" success=",observed_ipc_773.returns);
            up_field(" denied=",refused);arch_putc('\n');
            if(usr_select(&observed_sessions.users,observed_sessions.sid[1])!=USR_OK)
                up_fail("773 restore selected session");
        }
#endif
        frame_line(id,&slot->context,&event);
        for(unsigned n=0;n<UP_SLOTS;++n){named("DATA",n);up_field(" value=",private_data(n));arch_putc('\n');}
        arch_puts("STYLEOS USER SAFE");up_field(" root=",now.root);up_field(" sp=",current_sp());arch_putc('\n');
        check(up_accept(&d->control,event.kind,now.root));
        const enum ps_event observed=event.kind==UP_YIELD?PS_YIELD:
                    event.kind==UP_EXIT?PS_EXIT:PS_FAULT;
        pscheck(ps_accept(&observed_scheduler,id,observed_handle[id],
                           event.active_root,now.root,observed));
#if defined(STYLEOS_TEST_NATIVE_TEA_SESSION) || defined(STYLEOS_TEST_USER_IPC_773)
        if(!nsh_on_event(&observed_sessions,id,observed))up_fail("session event");
        if(observed==PS_FAULT)arch_puts("STYLEOS TEA770 USER A FAULT WINDOW REAP PASS\n");
        if(observed==PS_EXIT)arch_puts("STYLEOS TEA770 USER B EXIT WINDOW REAP PASS\n");
#endif
#ifdef STYLEOS_TEST_USER_IPC_773
        if((observed==PS_FAULT && nwc_reconcile(&observed_ipc_773.channel)!=1) ||
           (observed==PS_EXIT && nwc_port_count(&observed_ipc_773.channel)!=0))
            up_fail("773 orphan window port");
#endif
        const enum pr_state expected=observed==PS_YIELD?PR_READY:
                      observed==PS_EXIT?PR_EXITED:PR_FAULTED;
        if(pr_find(&observed_registry,pid)->state!=expected ||
           pr_snapshot(&observed_registry,0,0,false)!=(turn<2?2u:turn==2?1u:0u))
           up_fail("registry event");
        if(turn==1){d->before_b=slot->context;d->b_before=private_data(1);}
        if(turn==2)arch_puts("STYLEOS USER TERMINATED id=A state=FAULTED\n");
    }
    unsigned unused=UP_SLOTS;
    if(up_go(&d->control,&unused)!=UP_EMPTY || unused!=UP_SLOTS || d->control.phase!=4 ||
       d->control.slots[0].state!=UP_FAULTED || d->control.slots[1].state!=UP_EXITED ||
       d->control.dispatches!=4 || d->control.slots[0].yields!=1 || d->control.slots[1].yields!=1)up_fail("completion");
    d->b_after=private_data(1);if(d->b_before!=UP_DATA_BEFORE || d->b_after!=UP_DATA_AFTER)up_fail("B survived");
    arch_puts("STYLEOS USER EMPTY");up_field(" faulted=",1);up_field(" exited=",1);up_field(" dispatches=",4);arch_putc('\n');
    for(unsigned id=0;id<UP_SLOTS;++id){
        struct up_machine now;check(up_arch_observe(&now));
        check(up_release(&d->control,id,now.root));
        pscheck(ps_reap(&observed_scheduler,id,observed_handle[id]));
        named("RELEASE",id);arch_puts(" state=FREE");up_field(" root=",now.root);arch_putc('\n');
    }
    if(up_go(&d->control,&unused)!=UP_EMPTY)up_fail("released context reused");
    unsigned ignored=UP_SLOTS;uint32_t ignored_pid=0;
    if(ps_next(&observed_scheduler,&ignored,&ignored_pid)!=PS_EMPTY ||
       pr_snapshot(&observed_registry,0,0,false)!=0 ||
       observed_scheduler.dispatches!=4)up_fail("scheduler completion");
    struct up_machine final;check(up_arch_observe(&final));
    if(!machine_same(&initial,&final) || dispatcher_cookie!=UP_DISPATCH_COOKIE)up_fail("kernel final check");
#if defined(STYLEOS_TEST_NATIVE_TEA_SESSION) || defined(STYLEOS_TEST_USER_IPC_773)
    if(!nsh_finish(&observed_sessions))up_fail("session final");
    arch_puts("STYLEOS TEA770 PID ROOT SESSION WINDOW PASS\n");
#endif
#ifdef STYLEOS_TEST_USER_SYSCALL
    arch_puts("STYLEOS UC772 KERNEL-TRAP COPY PROOF PASS\n");
#endif
#ifdef STYLEOS_TEST_USER_IPC_773
    arch_puts("STYLEOS IPC773 RETURNING CPL3 WINDOW IPC PASS\n");
#endif
    machine_line("FINAL",&final);arch_puts("STYLEOS USER PASS\n");
#ifdef STYLEOS_TEST_NATIVE_TEA
#ifdef STYLEOS_TEST_NATIVE_TEA_DISK
    arch_puts("STYLEOS TEA769 DISK CPL3 EXIT PASS\n");
#else
    arch_puts("STYLEOS TEA768 CPL3 EXIT PASS\n");
#endif
#endif
}
