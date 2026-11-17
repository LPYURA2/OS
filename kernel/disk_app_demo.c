/* T-P1.8B-only integration of the unchanged T-P1.8A store/transport with
 * separately validated external code and the new finite EL0 adapter. */
#include "styleos/arch.h"
#include "styleos/boot_pool.h"
#include "styleos/disk_app_demo.h"
#include "styleos/disk_app_arch.h"
#include "styleos/storage.h"
#include "styleos/virtio_block.h"

_Alignas(4096) uint64_t styleos_disk_app_tables[DA_ROOTS][DA_TABLE_PAGES][512];
_Alignas(4096) unsigned char styleos_disk_app_control[DA_CONTROL_BYTES];
_Alignas(4096) unsigned char styleos_disk_app_entry_stack[DA_ENTRY_STACK_BYTES];
_Alignas(4096) unsigned char styleos_disk_app_code[DA_PAGE];
_Alignas(4096) unsigned char styleos_disk_app_user_stack[DA_PAGE];
_Alignas(4096) union st_workspace styleos_disk_app_storage_workspace;
_Static_assert(sizeof styleos_disk_app_tables==262144,"complete finite table reserve");

extern const unsigned char image_start[],__image_end[],aarch64_vectors[];
static const unsigned char filename[]="app.bin";
enum slot_state { SLOT_FREE, SLOT_READY, SLOT_RUNNING, SLOT_FAULTED, SLOT_EXITED };
struct guest_state {
    struct st_store store;
    struct st_record record;
    struct da_info info;
    struct da_platform platform;
    struct da_context context;
    struct da_event event;
    uint64_t reads,writes,flushes,requests;
    unsigned transport_error,io_open,denied;
    enum slot_state slot;
    unsigned dispatches,releases;
};
_Static_assert(sizeof(struct guest_state)<=DA_ARCH_CONTROL_OFFSET,
               "guest state does not overlap architecture scratch");

_Noreturn void da_fail(const char *why)
{
    arch_puts("STYLEOS DISKAPP ERROR reason=");arch_puts(why);arch_putc('\n');arch_halt();
}
void da_hex(uint64_t value)
{
    arch_puts("0x");for(int shift=60;shift>=0;shift-=4)
        arch_putc("0123456789abcdef"[(value>>shift)&15u]);
}
void da_field(const char *name,uint64_t value){arch_puts(name);da_hex(value);}
void da_range_line(const char *name,struct boot_range r)
{
    arch_puts("STYLEOS DISKAPP ");arch_puts(name);arch_putc(' ');
    da_hex(r.start);arch_putc(' ');da_hex(r.end);arch_putc('\n');
}
static void raw_hex(const unsigned char *data,size_t bytes)
{
    for(size_t i=0;i<bytes;++i){arch_putc("0123456789abcdef"[data[i]>>4]);
        arch_putc("0123456789abcdef"[data[i]&15u]);}
}
static void clear(void *p,size_t bytes)
{unsigned char *d=p;for(size_t i=0;i<bytes;++i)d[i]=0;}
static int same(const void *a,const void *b,size_t bytes)
{
    const unsigned char *x=a,*y=b;for(size_t i=0;i<bytes;++i)if(x[i]!=y[i])return 0;return 1;
}
static int inside(struct boot_range outer,struct boot_range inner)
{return inner.start<inner.end && outer.start<=inner.start && inner.end<=outer.end;}
static int overlap(struct boot_range a,struct boot_range b)
{return a.start<b.end && b.start<a.end;}
static struct boot_range extent(const void *p,size_t bytes)
{
    struct boot_range r;if(bm_extent((uintptr_t)p,bytes,&r)!=BM_OK)da_fail("extent");return r;
}
static int reserved(const struct boot_memory_evidence *e,struct boot_range r)
{
    if(!e || !e->map || e->map->nr_reserved>BOOT_MEMORY_INPUT_MAX ||
       e->map->nr_candidates>BOOT_MEMORY_RANGE_MAX)return 0;
    int found=0;
    for(size_t i=0;i<e->map->nr_reserved;++i)if(inside(e->map->reserved[i],r))found=1;
    for(size_t i=0;i<e->map->nr_candidates;++i)if(overlap(e->map->candidates[i],r))return 0;
    return found;
}
static uint64_t current_sp(void)
{uint64_t sp;__asm__ volatile("mov %0,sp":"=r"(sp));return sp;}
static void machine_line(const char *phase,const struct da_machine *m)
{
    arch_puts("STYLEOS DISKAPP MACHINE phase=");arch_puts(phase);
    da_field(" root=",m->root);da_field(" mode=",m->mode);
    da_field(" control0=",m->control0);da_field(" control1=",m->control1);
    da_field(" control2=",m->control2);da_field(" control3=",m->control3);
    da_field(" vectors=",m->vectors);da_field(" upper_root=",m->upper_root);
    da_field(" active=",m->active);da_field(" dispatcher_sp=",m->dispatcher_sp);
    da_field(" context_live=",m->context_live);da_field(" sp=",current_sp());arch_putc('\n');
}
static void admit(const struct boot_memory_evidence *e,struct da_machine *m)
{
    struct boot_range pool=boot_pool_storage_range(),metadata=boot_pool_metadata_range();
    if(boot_pool_check_layout(pool.start,pool.end-pool.start,metadata.start,
                             metadata.end-metadata.start,e)!=BP_OK)da_fail("evidence");
    uint64_t spsel;
    __asm__ volatile("mrs %0,CurrentEL":"=r"(m->control2));
    __asm__ volatile("mrs %0,sctlr_el1":"=r"(m->mode));
    __asm__ volatile("mrs %0,DAIF":"=r"(m->control3));
    __asm__ volatile("mrs %0,vbar_el1":"=r"(m->vectors));
    __asm__ volatile("mrs %0,ttbr0_el1":"=r"(m->root));
    __asm__ volatile("mrs %0,ttbr1_el1":"=r"(m->upper_root));
    __asm__ volatile("mrs %0,tcr_el1":"=r"(m->control0));
    __asm__ volatile("mrs %0,mair_el1":"=r"(m->control1));
    __asm__ volatile("mrs %0,SPSel":"=r"(spsel));
    m->active=UINT64_MAX;m->dispatcher_sp=0;m->context_live=0;
    uint64_t sp=current_sp();
    if(m->control2!=4 || spsel!=1 || m->control3!=UINT64_C(0x3c0) || (sp&15u) ||
       !inside(e->live.stack,extent((const void *)(uintptr_t)sp,16)) ||
       (m->mode&((UINT64_C(1)<<25)|(UINT64_C(1)<<24)|(UINT64_C(1)<<12)|5u)) ||
       e->live.image.start!=UINT64_C(0x40080000) ||
       e->live.image.start!=(uint64_t)(uintptr_t)image_start ||
       e->live.image.end!=(uint64_t)(uintptr_t)__image_end ||
       m->vectors!=(uint64_t)(uintptr_t)aarch64_vectors ||
       !inside(e->live.image,extent(aarch64_vectors,2048)) ||
       !reserved(e,e->live.image))da_fail("IO-platform");
    const volatile uint32_t *h32=(const volatile uint32_t *)(uintptr_t)image_start;
    const volatile uint64_t *h64=(const volatile uint64_t *)(uintptr_t)image_start;
    if(h32[14]!=UINT32_C(0x644d5241) || h64[1]!=UINT64_C(0x80000) ||
       h64[2]!=e->live.image.end-e->live.image.start)da_fail("Image-header");
    struct boot_range vectors=extent(styleos_disk_app_vectors,2048);
    if((vectors.start&2047u) ||
       (uintptr_t)styleos_disk_app_vectors_end!=(uintptr_t)styleos_disk_app_vectors+2048 ||
       !inside(e->live.image,vectors) || !reserved(e,vectors))da_fail("vectors");
    const struct boot_range own[]={
        extent(styleos_disk_app_tables,sizeof styleos_disk_app_tables),
        extent(styleos_disk_app_control,sizeof styleos_disk_app_control),
        extent(styleos_disk_app_entry_stack,sizeof styleos_disk_app_entry_stack),
        extent(styleos_disk_app_code,sizeof styleos_disk_app_code),
        extent(styleos_disk_app_user_stack,sizeof styleos_disk_app_user_stack),
        extent(&styleos_disk_app_storage_workspace,sizeof styleos_disk_app_storage_workspace),
        extent(styleos_vblk_queue,sizeof styleos_vblk_queue),
        extent(styleos_vblk_request,sizeof styleos_vblk_request),
        extent(&styleos_vblk_control,sizeof styleos_vblk_control)};
    const struct boot_range excluded[]={e->live.stack,e->live.handoff,e->live.source,
        e->workspace,pool,metadata,extent(aarch64_vectors,2048),vectors};
    for(size_t j=0;j+1<sizeof excluded/sizeof excluded[0];++j)
        if(overlap(vectors,excluded[j]))da_fail("vector-live-overlap");
    for(size_t i=0;i<sizeof own/sizeof own[0];++i){
        if((own[i].start|own[i].end)&4095u || !inside(e->live.image,own[i]) ||
           !reserved(e,own[i]))da_fail("owned-range");
        for(size_t j=0;j<i;++j)if(overlap(own[i],own[j]))da_fail("owned-overlap");
        for(size_t j=0;j<sizeof excluded/sizeof excluded[0];++j)
            if(overlap(own[i],excluded[j]))da_fail("live-overlap");
    }
    /* The complete mutable closure, full BSS/Image and immutable vectors have
     * been proved before initialization. Only fixed Image pointers become PAs. */
}
static void objects(const struct boot_memory_evidence *e)
{
    da_range_line("OBJECT name=tables",extent(styleos_disk_app_tables,sizeof styleos_disk_app_tables));
    da_range_line("OBJECT name=control",extent(styleos_disk_app_control,sizeof styleos_disk_app_control));
    da_range_line("OBJECT name=entry_stack",extent(styleos_disk_app_entry_stack,sizeof styleos_disk_app_entry_stack));
    da_range_line("OBJECT name=code",extent(styleos_disk_app_code,sizeof styleos_disk_app_code));
    da_range_line("OBJECT name=user_stack",extent(styleos_disk_app_user_stack,sizeof styleos_disk_app_user_stack));
    da_range_line("OBJECT name=workspace",extent(&styleos_disk_app_storage_workspace,sizeof styleos_disk_app_storage_workspace));
    da_range_line("OBJECT name=queue",extent(styleos_vblk_queue,sizeof styleos_vblk_queue));
    da_range_line("OBJECT name=request",extent(styleos_vblk_request,sizeof styleos_vblk_request));
    da_range_line("OBJECT name=vblk_control",extent(&styleos_vblk_control,sizeof styleos_vblk_control));
    da_range_line("OBJECT name=vectors",extent(styleos_disk_app_vectors,2048));
    da_range_line("OBJECT name=old_vectors",extent(aarch64_vectors,2048));
    da_range_line("OBJECT name=stack",e->live.stack);da_range_line("OBJECT name=handoff",e->live.handoff);
    da_range_line("OBJECT name=source",e->live.source);da_range_line("OBJECT name=diagnostic",e->workspace);
    da_range_line("OBJECT name=pool",boot_pool_storage_range());
    da_range_line("OBJECT name=pool_metadata",boot_pool_metadata_range());
}
/* These are trusted adapters, not an application file API. No callback can
 * issue WRITE/FLUSH, and closing io_open permanently prevents further reads. */
static int block_read(void *context,uint64_t lba,void *data)
{
    struct guest_state *d=context;
    if(!d->io_open){++d->denied;return 1;}
    enum vb_error error=vb_read(lba,data);d->transport_error=(unsigned)error;return error!=VB_OK;
}
static int block_write(void *context,uint64_t lba,const void *data)
{struct guest_state *d=context;(void)lba;(void)data;++d->denied;return 1;}
static int block_flush(void *context)
{struct guest_state *d=context;++d->denied;return 1;}
static void store_check(enum st_error error,const struct guest_state *d)
{
    if(error!=ST_OK){
        if(error==ST_IO)da_fail(vb_error_name((enum vb_error)d->transport_error));
        da_fail(st_error_name(error));
    }
}
static void drain(struct guest_state *d)
{
    d->io_open=0;
    __asm__ volatile("dsb sy":::"memory");
    d->reads=vb_reads();d->writes=vb_writes();d->flushes=vb_flushes();d->requests=vb_requests();
    volatile uint16_t *avail=(volatile uint16_t *)(void *)(styleos_vblk_queue+VB_AVAIL_OFFSET+2u);
    volatile uint16_t *used=(volatile uint16_t *)(void *)(styleos_vblk_queue+VB_USED_OFFSET+2u);
    uint16_t a=*avail,u=*used;
    if(d->denied || !d->requests || d->requests>VB_MAX_REQUESTS ||
       d->requests!=d->reads || d->writes || d->flushes ||
       a!=d->requests || u!=d->requests)da_fail("DMA-not-drained");
    __asm__ volatile("dsb sy\nisb":::"memory");
    if(*avail!=a || *used!=u)da_fail("DMA-not-stable");
    arch_puts("STYLEOS DISKAPP DRAIN");da_field(" reads=",d->reads);
    da_field(" writes=",d->writes);da_field(" flushes=",d->flushes);
    da_field(" requests=",d->requests);da_field(" avail=",a);da_field(" used=",u);arch_putc('\n');
    /* No transport getter, device register, request, notify or reset follows
     * this point. The unchanged driver completed each one-outstanding request;
     * bounded equal raw indices prove no published descriptor remains in flight. */
}
static void arch_check(enum da_arch_result error)
{if(error!=DA_ARCH_OK)da_fail(da_arch_error_name(error));}
static int machine_same(const struct da_machine *a,const struct da_machine *b)
{
    return a->root==b->root && a->mode==b->mode && a->control0==b->control0 &&
        a->control1==b->control1 && a->control2==b->control2 && a->control3==b->control3 &&
        a->vectors==b->vectors && a->upper_root==b->upper_root &&
        a->active==UINT64_MAX && a->dispatcher_sp==0 && !a->context_live;
}
static void run_one(struct guest_state *d,const struct boot_memory_evidence *e,unsigned arg)
{
    if(d->slot!=SLOT_FREE || d->dispatches!=arg || d->releases!=arg || d->io_open)
        da_fail("slot-create-state");
    struct da_machine before;arch_check(da_arch_observe(&before));
    if(!machine_same(&before,&d->platform.safe))da_fail("unsafe-before-create");
    /* A new fixed-slot activation starts with a fresh stack and full integer
     * context. The previous terminal context cannot be selected or resumed. */
    clear(styleos_disk_app_user_stack,sizeof styleos_disk_app_user_stack);
    clear(&d->context,sizeof d->context);clear(&d->event,sizeof d->event);
    d->context.pc=DA_CODE_VA+d->info.entry_offset;
    d->context.sp=DA_STACK_VA+DA_PAGE;d->context.status=UINT64_C(0x3c0);
    d->context.gpr[0]=arg;d->slot=SLOT_READY;
    arch_puts("STYLEOS DISKAPP RUN");da_field(" arg=",arg);da_field(" entry=",d->context.pc);
    da_field(" sp=",d->context.sp);da_field(" root=",d->platform.roots[DA_ROOT_USER]);arch_putc('\n');
    d->slot=SLOT_RUNNING;++d->dispatches;
    arch_check(da_arch_run(&d->context,&d->event));
    struct da_context *c=&d->context;struct da_event *v=&d->event;
    if(d->slot!=SLOT_RUNNING || v->active_root!=d->platform.roots[DA_ROOT_USER] ||
       c->sp!=DA_STACK_VA+DA_PAGE || c->status!=UINT64_C(0x3c0) || c->cs || c->ss ||
       v->vector!=8 || !inside(extent(styleos_disk_app_entry_stack,sizeof styleos_disk_app_entry_stack),
                             extent((const void *)(uintptr_t)v->kernel_sp,16)))da_fail("EL0-origin-context");
    if(arg==0){
        if(v->kind!=DA_EVENT_FAULT || c->pc!=DA_CODE_VA+d->info.fault_offset ||
           v->fault_va!=DA_FAULT_VA || (v->syndrome>>26)!=UINT64_C(0x24) ||
           (v->syndrome&UINT64_C(0x1ffffff))!=UINT64_C(0x6) ||
           c->gpr[0]!=0 || c->gpr[1]!=DA_FAULT_VA)da_fail("EL0-fault-proof");
        d->slot=SLOT_FAULTED;
    }else{
        if(v->kind!=DA_EVENT_EXIT || c->pc!=DA_CODE_VA+d->info.exit_offset+4u ||
           v->fault_va!=0 || (v->syndrome>>26)!=UINT64_C(0x15) ||
           (v->syndrome&UINT64_C(0x1ffffff))!=2 ||
           c->gpr[0]!=d->info.exit_code || c->gpr[19]!=d->info.marker)da_fail("EL0-exit-proof");
        d->slot=SLOT_EXITED;
    }
    arch_puts("STYLEOS DISKAPP EVENT");da_field(" arg=",arg);
    arch_puts(" kind=");arch_puts(arg?"exit":"fault");da_field(" pc=",c->pc);da_field(" sp=",c->sp);
    da_field(" status=",c->status);da_field(" vector=",v->vector);da_field(" syndrome=",v->syndrome);
    da_field(" fault_va=",v->fault_va);da_field(" root=",v->active_root);da_field(" kernel_sp=",v->kernel_sp);
    da_field(" x0=",c->gpr[0]);da_field(" x1=",c->gpr[1]);da_field(" x19=",c->gpr[19]);arch_putc('\n');
    struct da_machine after;arch_check(da_arch_observe(&after));
    uint64_t sp=current_sp();
    if(!machine_same(&after,&d->platform.safe) ||
       !inside(e->live.stack,extent((const void *)(uintptr_t)sp,16)))da_fail("unsafe-dispatcher-return");
    arch_puts("STYLEOS DISKAPP SAFE");da_field(" arg=",arg);da_field(" root=",after.root);
    da_field(" sp=",sp);arch_putc('\n');machine_line("SAFE",&after);
    /* Only after hardware SAFE/original-stack observations may bookkeeping be
     * cleared. Stack/code backing remains admitted static image storage. */
    clear(&d->context,sizeof d->context);clear(&d->event,sizeof d->event);
    d->slot=SLOT_FREE;++d->releases;
    arch_puts("STYLEOS DISKAPP RELEASE");da_field(" arg=",arg);arch_puts(" state=FREE\n");
}
void disk_app_demo_run(const struct boot_memory_evidence *e)
{
    struct da_machine m;admit(e,&m);
    struct disk_app_boot boot;enum disk_app_fdt_error fe=disk_app_fdt_parse(
        (const void *)(uintptr_t)e->live.source.start,
        (size_t)(e->live.source.end-e->live.source.start),&boot);
    if(fe!=DAF_OK)da_fail(disk_app_fdt_error_name(fe));
    arch_puts("STYLEOS DISKAPP PHASE name=");arch_puts(disk_app_scenario_name(boot.scenario));arch_putc('\n');
    objects(e);machine_line("IO",&m);
    arch_puts("STYLEOS DISKAPP FDT");da_field(" source=",e->live.source.start);
    da_field(" bytes=",e->live.source.end-e->live.source.start);da_field(" candidates=",boot.count);arch_putc('\n');
    struct vb_mmio candidates[DISK_APP_MMIO_MAX];
    for(size_t i=0;i<boot.count;++i)candidates[i]=(struct vb_mmio){boot.mmio[i].base,boot.mmio[i].bytes};
    enum vb_error ve=vb_open(e,candidates,boot.count);if(ve!=VB_OK)da_fail(vb_error_name(ve));
    if(vb_capacity()!=ST_DISK_SECTORS || vb_features()!=VB_REQUIRED_FEATURES ||
       vb_queue_size()!=VB_QUEUE_SIZE)da_fail("fixed-device-profile");
    arch_puts("STYLEOS DISKAPP DEVICE");da_field(" base=",vb_mmio_base());da_field(" bytes=",UINT64_C(0x200));
    da_field(" version=",2);da_field(" id=",2);da_field(" features=",vb_offered_features());
    da_field(" negotiated=",vb_features());da_field(" sectors=",vb_capacity());da_field(" queue=",vb_queue_size());arch_putc('\n');
    clear(styleos_disk_app_control,sizeof styleos_disk_app_control);
    struct guest_state *d=(struct guest_state *)(void *)styleos_disk_app_control;
    d->io_open=1;
    const struct st_io io={d,vb_capacity(),block_read,block_write,block_flush};
    store_check(st_mount(&io,&styleos_disk_app_storage_workspace,&d->store),d);
    store_check(st_read(&d->store,&styleos_disk_app_storage_workspace,&d->record),d);
    const struct st_record *r=&d->record;
    if(!r->generation || r->name_length!=sizeof filename-1 ||
       !same(r->name,filename,sizeof filename-1) || !r->length || r->length>DA_FILE_MAX)
        da_fail("app-file-identity");
    arch_puts("STYLEOS DISKAPP READ");da_field(" generation=",r->generation);
    arch_puts(" name=");raw_hex(r->name,r->name_length);da_field(" length=",r->length);
    arch_puts(" data=");raw_hex(r->data,r->length);arch_putc('\n');
    drain(d);
    enum da_result result=da_validate(r->data,r->length,&d->info);
    if(result!=DA_OK){
        if(boot.scenario!=DISK_APP_REJECT)da_fail(da_result_name(result));
        arch_puts("STYLEOS DISKAPP REJECT reason=");arch_puts(da_result_name(result));arch_putc('\n');return;
    }
    if(boot.scenario!=DISK_APP_RUN)da_fail("reject-fixture-was-valid");
    arch_puts("STYLEOS DISKAPP FORMAT");da_field(" file=",r->length);da_field(" code=",d->info.code_length);
    da_field(" entry=",d->info.entry_offset);da_field(" fault=",d->info.fault_offset);
    da_field(" exit=",d->info.exit_offset);da_field(" marker=",d->info.marker);
    da_field(" exit_code=",d->info.exit_code);da_field(" checksum=",d->info.checksum);arch_putc('\n');
    clear(styleos_disk_app_code,sizeof styleos_disk_app_code);
    result=da_copy(r->data,r->length,styleos_disk_app_code,sizeof styleos_disk_app_code,&d->info);
    if(result!=DA_OK)da_fail(da_result_name(result));
    arch_puts("STYLEOS DISKAPP COPY");da_field(" source=",(uintptr_t)r->data+DA_HEADER_BYTES);
    da_field(" destination=",(uintptr_t)styleos_disk_app_code);da_field(" bytes=",d->info.code_length);
    da_field(" crc=",st_crc32(styleos_disk_app_code,sizeof styleos_disk_app_code));da_field(" va=",DA_CODE_VA);
    arch_puts(" data=");raw_hex(styleos_disk_app_code,sizeof styleos_disk_app_code);arch_putc('\n');
    arch_check(da_arch_prepare(e,&d->info,&d->platform));
    if(d->io_open || d->denied)da_fail("IO-transition-state");
    machine_line("SAFE",&d->platform.safe);da_arch_dump(&d->platform);
    run_one(d,e,0);run_one(d,e,1);
    if(d->slot!=SLOT_FREE || d->dispatches!=2 || d->releases!=2 || d->io_open || d->denied)
        da_fail("final-slot-state");
    arch_puts("STYLEOS DISKAPP PASS\n");
}
