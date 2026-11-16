#include "styleos/arch.h"
#include "styleos/boot_pool.h"
#include "styleos/storage_demo.h"
#include "styleos/virtio_block.h"

_Alignas(4096) union storage_demo_control styleos_storage_control;
_Alignas(4096) union st_workspace styleos_storage_workspace;
extern const unsigned char image_start[],__image_end[],aarch64_vectors[];

static const unsigned char filename[]="note.txt";
static const unsigned char content_a[]="StyleOS note A\n";
static const unsigned char content_b[]="StyleOS note B: updated\n";
_Static_assert(sizeof content_a-1==15,"exact first version");
_Static_assert(sizeof content_b-1==24,"exact updated version");

static void hex(uint64_t value)
{
    arch_puts("0x");for(int shift=60;shift>=0;shift-=4)
        arch_putc("0123456789abcdef"[(value>>shift)&15u]);
}
static void field(const char *name,uint64_t value){arch_puts(name);hex(value);}
static void raw_hex(const unsigned char *data,size_t bytes)
{
    for(size_t i=0;i<bytes;++i){arch_putc("0123456789abcdef"[data[i]>>4]);arch_putc("0123456789abcdef"[data[i]&15u]);}
}
static _Noreturn void fail(const char *stage,const char *why)
{
    arch_puts("STYLEOS STORAGE ERROR stage=");arch_puts(stage);
    arch_puts(" reason=");arch_puts(why);arch_putc('\n');arch_halt();
}
static int inside(struct boot_range outer,struct boot_range inner)
{return inner.start<inner.end && outer.start<=inner.start && inner.end<=outer.end;}
static int overlap(struct boot_range a,struct boot_range b)
{return a.start<b.end && b.start<a.end;}
static struct boot_range extent(const void *p,size_t bytes)
{
    struct boot_range r;if(bm_extent((uintptr_t)p,bytes,&r)!=BM_OK)fail("admit","overflow");return r;
}
static void object(const char *name,struct boot_range r)
{
    arch_puts("STYLEOS STORAGE OBJECT name=");arch_puts(name);arch_putc(' ');
    hex(r.start);arch_putc(' ');hex(r.end);arch_putc('\n');
}
static int reserved(const struct boot_memory_evidence *e,struct boot_range r)
{
    if(e->map->nr_reserved>BOOT_MEMORY_INPUT_MAX || e->map->nr_candidates>BOOT_MEMORY_RANGE_MAX)return 0;
    int found=0;for(size_t i=0;i<e->map->nr_reserved;++i)if(inside(e->map->reserved[i],r))found=1;
    for(size_t i=0;i<e->map->nr_candidates;++i)if(overlap(e->map->candidates[i],r))return 0;
    return found;
}
struct machine {uint64_t el,sctlr,daif,sp,vectors;};
static void admit(const struct boot_memory_evidence *e,struct machine *m)
{
    struct boot_range pool=boot_pool_storage_range(),metadata=boot_pool_metadata_range();
    if(boot_pool_check_layout(pool.start,pool.end-pool.start,metadata.start,
                             metadata.end-metadata.start,e)!=BP_OK)fail("admit","evidence");
    uint64_t spsel;
    __asm__ volatile("mrs %0,CurrentEL":"=r"(m->el));
    __asm__ volatile("mrs %0,sctlr_el1":"=r"(m->sctlr));
    __asm__ volatile("mrs %0,DAIF":"=r"(m->daif));
    __asm__ volatile("mov %0,sp":"=r"(m->sp));
    __asm__ volatile("mrs %0,vbar_el1":"=r"(m->vectors));
    __asm__ volatile("mrs %0,SPSel":"=r"(spsel));
    if(m->el!=4 || spsel!=1 || m->daif!=UINT64_C(0x3c0) || (m->sp&15u) ||
       !inside(e->live.stack,extent((const void *)(uintptr_t)m->sp,16)) ||
       (m->sctlr&((UINT64_C(1)<<25)|(UINT64_C(1)<<24)|(UINT64_C(1)<<12)|5u)) ||
       e->live.image.start!=UINT64_C(0x40080000) ||
       e->live.image.start!=(uint64_t)(uintptr_t)image_start ||
       e->live.image.end!=(uint64_t)(uintptr_t)__image_end ||
       m->vectors!=(uint64_t)(uintptr_t)aarch64_vectors ||
       !inside(e->live.image,extent(aarch64_vectors,2048)) || !reserved(e,e->live.image))fail("admit","platform");
    const volatile uint32_t *h32=(const volatile uint32_t *)(uintptr_t)image_start;
    const volatile uint64_t *h64=(const volatile uint64_t *)(uintptr_t)image_start;
    if(h32[14]!=UINT32_C(0x644d5241) || h64[1]!=UINT64_C(0x80000) ||
       h64[2]!=e->live.image.end-e->live.image.start)fail("admit","Image header");
    struct boot_range own[]={extent(&styleos_storage_control,sizeof styleos_storage_control),
        extent(&styleos_storage_workspace,sizeof styleos_storage_workspace),
        extent(styleos_vblk_queue,sizeof styleos_vblk_queue),
        extent(styleos_vblk_request,sizeof styleos_vblk_request),
        extent(&styleos_vblk_control,sizeof styleos_vblk_control)};
    struct boot_range excluded[]={e->live.stack,e->live.handoff,e->live.source,e->workspace,
                                  pool,metadata,extent(aarch64_vectors,2048)};
    for(size_t i=0;i<sizeof own/sizeof own[0];++i){
        if((own[i].start|own[i].end)&4095u || !inside(e->live.image,own[i]) ||
           !reserved(e,own[i]))fail("admit","owned range");
        for(size_t j=0;j<i;++j)if(overlap(own[i],own[j]))fail("admit","owned overlap");
        for(size_t j=0;j<sizeof excluded/sizeof excluded[0];++j)
            if(overlap(own[i],excluded[j]))fail("admit","live overlap");
    }
    /* No new persistent object has been written. All pointer-to-PA uses below
     * are confined to this fixed, demonstrated MMU-off Image identity. */
}
static void objects(const struct boot_memory_evidence *e)
{
    object("control",extent(&styleos_storage_control,sizeof styleos_storage_control));
    object("workspace",extent(&styleos_storage_workspace,sizeof styleos_storage_workspace));
    object("queue",extent(styleos_vblk_queue,sizeof styleos_vblk_queue));
    object("request",extent(styleos_vblk_request,sizeof styleos_vblk_request));
    object("vblk_control",extent(&styleos_vblk_control,sizeof styleos_vblk_control));
    object("stack",e->live.stack);object("diagnostic",e->workspace);
    object("pool",boot_pool_storage_range());object("pool_metadata",boot_pool_metadata_range());
    object("vectors",extent(aarch64_vectors,2048));
}
static int block_read(void *context,uint64_t lba,void *data)
{
    enum vb_error error=vb_read(lba,data);*(unsigned *)context=(unsigned)error;return error!=VB_OK;
}
static int block_write(void *context,uint64_t lba,const void *data)
{
    enum vb_error error=vb_write(lba,data);*(unsigned *)context=(unsigned)error;return error!=VB_OK;
}
static int block_flush(void *context)
{
    enum vb_error error=vb_flush();*(unsigned *)context=(unsigned)error;return error!=VB_OK;
}
static void check(enum st_error error,struct storage_demo_state *d)
{
    if(error!=ST_OK){
        if(error==ST_IO)fail("transport",vb_error_name((enum vb_error)d->transport_error));
        fail("store",st_error_name(error));
    }
}
static int same(const unsigned char *a,const unsigned char *b,size_t bytes)
{for(size_t i=0;i<bytes;++i)if(a[i]!=b[i])return 0;return 1;}
static void read_version(struct storage_demo_state *d,const unsigned char *want,size_t bytes,uint64_t generation)
{
    check(st_read(&d->store,&styleos_storage_workspace,&d->record),d);
    struct st_record *r=&d->record;
    if(r->generation!=generation || r->name_length!=sizeof filename-1 ||
       r->length!=bytes || !same(r->name,filename,sizeof filename-1) ||
       !same(r->data,want,bytes))fail("read","exact file mismatch");
    arch_puts("STYLEOS STORAGE READ");field(" generation=",r->generation);
    arch_puts(" name=");raw_hex(r->name,r->name_length);field(" length=",r->length);
    arch_puts(" data=");raw_hex(r->data,r->length);arch_putc('\n');
}
static void progress(const char *name,const struct st_pending *p)
{
    arch_puts("STYLEOS STORAGE ");arch_puts(name);field(" generation=",p->generation);
    field(" length=",p->length);field(" payload_crc=",p->payload_crc);
    field(" reads=",vb_reads());field(" writes=",vb_writes());field(" flushes=",vb_flushes());arch_putc('\n');
}
static void io_line(void)
{
    arch_puts("STYLEOS STORAGE IO");field(" reads=",vb_reads());field(" writes=",vb_writes());
    field(" flushes=",vb_flushes());field(" requests=",vb_requests());arch_putc('\n');
}
static void prepare(struct storage_demo_state *d,const unsigned char *data,size_t bytes,uint64_t generation)
{
    uint64_t writes=vb_writes(),flushes=vb_flushes();
    check(st_prepare(&d->store,&styleos_storage_workspace,filename,sizeof filename-1,
                     data,bytes,&d->pending),d);
    if(d->pending.generation!=generation || d->pending.length!=bytes ||
       vb_writes()!=writes+4 || vb_flushes()!=flushes+2)fail("prepare","confirmed invalidation and data flush");
    progress("PREPARED",&d->pending);
}
static void commit(struct storage_demo_state *d)
{
    uint64_t writes=vb_writes(),flushes=vb_flushes();
    check(st_commit(&d->store,&styleos_storage_workspace,&d->pending),d);
    if(vb_writes()!=writes+1 || vb_flushes()!=flushes+1)fail("commit","confirmed final flush");
    progress("COMMITTED",&d->pending);
}
void storage_demo_run(const struct boot_memory_evidence *e)
{
    struct machine m;admit(e,&m);
    struct storage_boot boot;enum storage_fdt_error fe=storage_fdt_parse(
        (const void *)(uintptr_t)e->live.source.start,
        (size_t)(e->live.source.end-e->live.source.start),&boot);
    if(fe!=SF_OK)fail("FDT",storage_fdt_error_name(fe));
    arch_puts("STYLEOS STORAGE PHASE name=");arch_puts(storage_phase_name(boot.phase));arch_putc('\n');
    objects(e);
    arch_puts("STYLEOS STORAGE MACHINE");field(" el=",m.el);field(" sctlr=",m.sctlr);
    field(" daif=",m.daif);field(" sp=",m.sp);field(" vectors=",m.vectors);arch_putc('\n');
    arch_puts("STYLEOS STORAGE FDT");field(" source=",e->live.source.start);
    field(" bytes=",e->live.source.end-e->live.source.start);field(" candidates=",boot.count);arch_putc('\n');
    struct vb_mmio candidates[STORAGE_MMIO_MAX];
    for(size_t i=0;i<boot.count;++i)candidates[i]=(struct vb_mmio){boot.mmio[i].base,boot.mmio[i].bytes};
    enum vb_error ve=vb_open(e,candidates,boot.count);if(ve!=VB_OK)fail("device",vb_error_name(ve));
    if(vb_capacity()!=ST_DISK_SECTORS || vb_features()!=VB_REQUIRED_FEATURES ||
       vb_queue_size()!=VB_QUEUE_SIZE)fail("device","fixed profile");
    arch_puts("STYLEOS STORAGE DEVICE");field(" base=",vb_mmio_base());field(" bytes=",UINT64_C(0x200));
    field(" version=",2);field(" id=",2);field(" features=",vb_offered_features());
    field(" negotiated=",vb_features());field(" sectors=",vb_capacity());field(" queue=",vb_queue_size());arch_putc('\n');
    /* Whole five-buffer admission and the driver's independent proof have
     * completed. Initializing only this union cannot overwrite driver state. */
    styleos_storage_control=(union storage_demo_control){0};
    struct storage_demo_state *d=&styleos_storage_control.state;
    struct st_io io={&d->transport_error,vb_capacity(),block_read,block_write,block_flush};
    if(boot.phase==STORAGE_INIT_A){
        check(st_init(&io,&styleos_storage_workspace,&d->store),d);
        if(vb_writes()!=1 || vb_flushes()!=1)fail("init","confirmed empty format");
        arch_puts("STYLEOS STORAGE INIT EMPTY\n");
        prepare(d,content_a,sizeof content_a-1,1);commit(d);
        read_version(d,content_a,sizeof content_a-1,1);
    }else{
        check(st_mount(&io,&styleos_storage_workspace,&d->store),d);
        if(vb_writes() || vb_flushes())fail("mount","unexpected mutation");
        if(boot.phase==STORAGE_READ_B)read_version(d,content_b,sizeof content_b-1,2);
        else read_version(d,content_a,sizeof content_a-1,1);
        if(boot.phase==STORAGE_UPDATE_B || boot.phase==STORAGE_PREPARE_B){
            prepare(d,content_b,sizeof content_b-1,2);
            if(boot.phase==STORAGE_PREPARE_B){
                io_line();arch_puts("STYLEOS STORAGE BARRIER BEFORE_COMMIT\n");
                /* Permanent barrier: no queue notify, commit or device reset
                 * follows. The runner must kill this VM and start a new one. */
                arch_halt();
            }
            commit(d);read_version(d,content_b,sizeof content_b-1,2);
        }
    }
    if(vb_requests()!=vb_reads()+vb_writes()+vb_flushes())fail("IO","unaccounted request");
    io_line();arch_puts("STYLEOS STORAGE PASS\n");
}
