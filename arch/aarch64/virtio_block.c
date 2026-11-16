/* T-P1.8A only. Polling modern VirtIO MMIO, one known QEMU virt block device.
 * No IRQ, allocator, arbitrary DMA, caches, translation change or old pool use. */
#include "styleos/virtio_block.h"
#ifndef STYLEOS_VBLK_HOST_TEST
#include "styleos/boot_pool.h"
#endif

#define MMIO_FIRST UINT64_C(0x0a000000)
#define MMIO_END UINT64_C(0x0a004000)
#define MMIO_SPAN UINT64_C(0x200)
#define MMIO_MAGIC UINT32_C(0x74726976)
#define F_RO (UINT32_C(1)<<5)
#define F_FLUSH (UINT32_C(1)<<9)
#define STATUS_ACK 1u
#define STATUS_DRIVER 2u
#define STATUS_DRIVER_OK 4u
#define STATUS_FEATURES_OK 8u
#define STATUS_NEEDS_RESET 64u
#define STATUS_FAILED 128u
#define DESC_NEXT 1u
#define DESC_WRITE 2u
#define VB_CTL_MAGIC UINT64_C(0x5354594c56424c4b)

struct vb_descriptor { uint64_t address; uint32_t length; uint16_t flags,next; };
struct vb_used_element { uint32_t id,length; };
struct vb_header { uint32_t type,reserved; uint64_t sector; };
_Static_assert(sizeof(struct vb_descriptor)==16,"split descriptor extent");
_Static_assert(sizeof(struct vb_header)==16,"virtio block request extent");
struct vb_bus {
    void *context;
    uint32_t (*read)(void *,uint32_t);
    void (*write)(void *,uint32_t,uint32_t);
    void (*poll)(void *);
};
struct vb_control {
    struct vb_bus bus;
    uint64_t magic,mmio,capacity,features,offered,requests,reads,writes,flushes;
    struct boot_range image;
    uint32_t generation;
    uint16_t avail,used;
    unsigned ready,failed,busy;
};
_Static_assert(sizeof(struct vb_control)<=VB_PAGE,"full control extent");
_Alignas(4096) unsigned char styleos_vblk_queue[VB_PAGE];
_Alignas(4096) unsigned char styleos_vblk_request[VB_PAGE];
_Alignas(4096) union vb_control_storage styleos_vblk_control;
#define CTL ((struct vb_control *)(void *)styleos_vblk_control.bytes)

static void clear(void *p,size_t bytes)
{ unsigned char *b=p;for(size_t i=0;i<bytes;++i)b[i]=0; }
static void copy(void *to,const void *from,size_t bytes)
{ unsigned char *d=to;const unsigned char *s=from;for(size_t i=0;i<bytes;++i)d[i]=s[i]; }
static void barrier(void)
{
#if defined(__aarch64__)
    __asm__ volatile("dmb sy":::"memory");
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}
static void complete_barrier(void)
{
#if defined(__aarch64__)
    __asm__ volatile("dsb sy":::"memory");
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}
static uint32_t rd(uint32_t offset)
{ return CTL->bus.read(CTL->bus.context,offset); }
static void wr(uint32_t offset,uint32_t value)
{ CTL->bus.write(CTL->bus.context,offset,value); }
static int healthy(uint32_t required)
{
    uint32_t status=rd(0x70);
    return (status&required)==required && !(status&(STATUS_FAILED|STATUS_NEEDS_RESET));
}
static enum vb_error fatal(enum vb_error error)
{
    CTL->failed=1;CTL->ready=0;CTL->busy=0;
    wr(0x70,rd(0x70)|STATUS_FAILED);complete_barrier();return error;
}
static volatile uint16_t *avail_word(unsigned offset)
{ return (volatile uint16_t *)(void *)(styleos_vblk_queue+VB_AVAIL_OFFSET+offset); }
static volatile uint16_t *used_word(unsigned offset)
{ return (volatile uint16_t *)(void *)(styleos_vblk_queue+VB_USED_OFFSET+offset); }
static void address(uint32_t reg,const void *p)
{
    /* Only these two statically admitted arrays can reach this helper. Under
     * MMU-off fixed Image identity this is their demonstrated DMA address. */
    uint64_t a=(uint64_t)(uintptr_t)p;
    wr(reg,(uint32_t)a);wr(reg+4u,(uint32_t)(a>>32));
}
static enum vb_error initialize(const struct vb_bus *bus,uint64_t mmio,
                                struct boot_range image)
{
    if(bus->read(bus->context,0)!=MMIO_MAGIC || bus->read(bus->context,8)!=2u)
        return VB_DEVICE;
    if(bus->read(bus->context,4)!=2u)return VB_VERSION;
    clear(&styleos_vblk_control,sizeof styleos_vblk_control);
    CTL->bus=*bus;CTL->magic=VB_CTL_MAGIC;CTL->mmio=mmio;CTL->image=image;
    wr(0x70,0);complete_barrier();
    unsigned reset;
    for(reset=0;reset<VB_RESET_LIMIT;++reset){
        if(rd(0x70)==0)break;
        if(CTL->bus.poll)CTL->bus.poll(CTL->bus.context);
    }
    if(reset==VB_RESET_LIMIT)return fatal(VB_RESET);
    wr(0x70,STATUS_ACK);if(!healthy(STATUS_ACK))return fatal(VB_STATE);
    wr(0x70,STATUS_ACK|STATUS_DRIVER);
    if(!healthy(STATUS_ACK|STATUS_DRIVER))return fatal(VB_STATE);
    wr(0x14,0);uint32_t lo=rd(0x10);
    wr(0x14,1);uint32_t hi=rd(0x10);
    CTL->offered=(uint64_t)lo|((uint64_t)hi<<32);
    if(!(hi&1u) || !(lo&F_FLUSH))return fatal(VB_FEATURES);
    if(lo&F_RO)return fatal(VB_READ_ONLY);
    wr(0x24,0);wr(0x20,F_FLUSH);
    wr(0x24,1);wr(0x20,1u);
    CTL->features=VB_REQUIRED_FEATURES;
    wr(0x70,STATUS_ACK|STATUS_DRIVER|STATUS_FEATURES_OK);
    if(!healthy(STATUS_ACK|STATUS_DRIVER|STATUS_FEATURES_OK))return fatal(VB_FEATURES);
    wr(0x30,0);
    if(rd(0x44)!=0 || rd(0x34)<VB_QUEUE_SIZE)return fatal(VB_QUEUE);
    clear(styleos_vblk_queue,sizeof styleos_vblk_queue);
    clear(styleos_vblk_request,sizeof styleos_vblk_request);
    *avail_word(0)=1u; /* VIRTQ_AVAIL_F_NO_INTERRUPT; no event-index feature. */
    wr(0x38,VB_QUEUE_SIZE);
    address(0x80,styleos_vblk_queue+VB_DESC_OFFSET);
    address(0x90,styleos_vblk_queue+VB_AVAIL_OFFSET);
    address(0xa0,styleos_vblk_queue+VB_USED_OFFSET);
    complete_barrier();wr(0x44,1);
    if(rd(0x44)!=1u)return fatal(VB_QUEUE);
    unsigned tries;
    for(tries=0;tries<32u;++tries){
        uint32_t before=rd(0xfc);barrier();
        uint64_t capacity=rd(0x100);capacity|=(uint64_t)rd(0x104)<<32;
        barrier();uint32_t after=rd(0xfc);
        if(before==after){CTL->capacity=capacity;CTL->generation=after;break;}
    }
    if(tries==32u || !CTL->capacity || CTL->capacity>UINT64_MAX/VB_SECTOR_BYTES)
        return fatal(VB_CAPACITY);
    wr(0x70,STATUS_ACK|STATUS_DRIVER|STATUS_FEATURES_OK|STATUS_DRIVER_OK);
    if(!healthy(15u))return fatal(VB_STATE);
    CTL->ready=1;return VB_OK;
}

#ifndef STYLEOS_VBLK_HOST_TEST
extern const unsigned char image_start[],__image_end[],aarch64_vectors[];
static int inside(struct boot_range r,uint64_t start,uint64_t bytes)
{ return r.start<r.end && bytes && start>=r.start && start<r.end && bytes<=r.end-start; }
static int overlap(struct boot_range a,struct boot_range b)
{ return a.start<b.end && b.start<a.end; }
static int reserved(const struct boot_memory_evidence *e,struct boot_range r)
{
    if(!e->map || e->map->nr_reserved>BOOT_MEMORY_INPUT_MAX ||
       e->map->nr_candidates>BOOT_MEMORY_RANGE_MAX)return 0;
    int yes=0;
    for(size_t i=0;i<e->map->nr_reserved;++i)
        if(inside(e->map->reserved[i],r.start,r.end-r.start))yes=1;
    for(size_t i=0;i<e->map->nr_candidates;++i)
        if(overlap(e->map->candidates[i],r))return 0;
    return yes;
}
static int admit(const struct boot_memory_evidence *e)
{
#if defined(__aarch64__)
    if(!e || !e->map)return 0;
    uint64_t el,spsel,sp,sctlr,daif,vbar;
    __asm__ volatile("mrs %0,CurrentEL":"=r"(el));
    __asm__ volatile("mrs %0,SPSel":"=r"(spsel));
    __asm__ volatile("mov %0,sp":"=r"(sp));
    __asm__ volatile("mrs %0,sctlr_el1":"=r"(sctlr));
    __asm__ volatile("mrs %0,DAIF":"=r"(daif));
    __asm__ volatile("mrs %0,vbar_el1":"=r"(vbar));
    if(el!=4 || spsel!=1 || daif!=UINT64_C(0x3c0) || (sp&15u) ||
       !inside(e->live.stack,sp,16u) ||
       (sctlr&((UINT64_C(1)<<25)|(UINT64_C(1)<<24)|(UINT64_C(1)<<12)|5u)) ||
       e->live.image.start!=UINT64_C(0x40080000) ||
       e->live.image.start!=(uint64_t)(uintptr_t)image_start ||
       e->live.image.end!=(uint64_t)(uintptr_t)__image_end ||
       vbar!=(uint64_t)(uintptr_t)aarch64_vectors ||
       !inside(e->live.image,vbar,2048u) || !reserved(e,e->live.image))return 0;
    const volatile uint32_t *h32=(const volatile uint32_t *)(uintptr_t)image_start;
    const volatile uint64_t *h64=(const volatile uint64_t *)(uintptr_t)image_start;
    if(h32[14]!=UINT32_C(0x644d5241) || h64[1]!=UINT64_C(0x80000) ||
       h64[2]!=e->live.image.end-e->live.image.start)return 0;
    struct boot_range objects[]={
        {(uintptr_t)styleos_vblk_queue,(uintptr_t)styleos_vblk_queue+VB_PAGE},
        {(uintptr_t)styleos_vblk_request,(uintptr_t)styleos_vblk_request+VB_PAGE},
        {(uintptr_t)&styleos_vblk_control,(uintptr_t)&styleos_vblk_control+VB_PAGE}};
    struct boot_range exclude[]={e->live.stack,e->live.handoff,e->live.source,
        e->workspace,boot_pool_storage_range(),boot_pool_metadata_range(),{vbar,vbar+2048u}};
    for(size_t i=0;i<3;++i){
        if((objects[i].start|objects[i].end)&4095u ||
           !inside(e->live.image,objects[i].start,VB_PAGE) || !reserved(e,objects[i]))return 0;
        for(size_t j=0;j<i;++j)if(overlap(objects[i],objects[j]))return 0;
        for(size_t j=0;j<sizeof exclude/sizeof exclude[0];++j)
            if(overlap(objects[i],exclude[j]))return 0;
    }
    return 1;
#else
    (void)e;return 0;
#endif
}
static uint32_t mmio_read(void *p,uint32_t offset)
{ return *(volatile uint32_t *)((uintptr_t)p+offset); }
static void mmio_write(void *p,uint32_t offset,uint32_t value)
{ *(volatile uint32_t *)((uintptr_t)p+offset)=value; }
#endif

enum vb_error vb_open(const struct boot_memory_evidence *e,
                      const struct vb_mmio *candidates,size_t count)
{
#ifdef STYLEOS_VBLK_HOST_TEST
    (void)e;(void)candidates;(void)count;return VB_PLATFORM;
#else
    if(!candidates || !count || count>32u)return VB_ARGUMENT;
    if(!admit(e))return VB_MEMORY;
    /* Read-only discovery precedes every write to control/queue/request/MMIO. */
    for(size_t i=0;i<count;++i){
        uint64_t base=candidates[i].base,bytes=candidates[i].bytes;
        if(bytes!=MMIO_SPAN || base<MMIO_FIRST || base>MMIO_END-MMIO_SPAN ||
           (base-MMIO_FIRST)%MMIO_SPAN)return VB_RANGE;
        for(size_t j=0;j<i;++j)if(base==candidates[j].base)return VB_RANGE;
    }
    uint64_t chosen=0;
    for(size_t i=0;i<count;++i){
        void *p=(void *)(uintptr_t)candidates[i].base;
        if(mmio_read(p,0)!=MMIO_MAGIC)return VB_DEVICE;
        if(mmio_read(p,8)!=2u)continue;
        if(chosen)return VB_DEVICE;
        if(mmio_read(p,4)!=2u)return VB_VERSION;
        chosen=candidates[i].base;
    }
    if(!chosen)return VB_DEVICE;
    const struct vb_bus bus={(void *)(uintptr_t)chosen,mmio_read,mmio_write,0};
    return initialize(&bus,chosen,e->live.image);
#endif
}

static int buffer(const void *p)
{
    uint64_t start=(uint64_t)(uintptr_t)p;
    if(!p || start>UINT64_MAX-VB_SECTOR_BYTES ||
       start<CTL->image.start || start+VB_SECTOR_BYTES>CTL->image.end)return 0;
    const uint64_t starts[]={(uintptr_t)styleos_vblk_queue,
        (uintptr_t)styleos_vblk_request,(uintptr_t)&styleos_vblk_control};
    for(unsigned i=0;i<3;++i)
        if(start<starts[i]+VB_PAGE && starts[i]<start+VB_SECTOR_BYTES)return 0;
    return 1;
}
static enum vb_error request(uint32_t type,uint64_t sector,void *data)
{
    struct vb_control *s=CTL;
    if(s->magic!=VB_CTL_MAGIC || !s->ready || s->failed || s->busy)return VB_STATE;
    if(type!=4u && !buffer(data))return VB_ARGUMENT;
    if(type!=4u && sector>=s->capacity)return VB_RANGE;
    if(s->requests>=VB_MAX_REQUESTS)return VB_LIMIT;
    if(!healthy(15u))return fatal(VB_STATE);
    if(rd(0xfc)!=s->generation)return fatal(VB_CONFIG);
    s->busy=1;
    struct vb_header *header=(struct vb_header *)(void *)(styleos_vblk_request+VB_HEADER_OFFSET);
    header->type=type;header->reserved=0;header->sector=type==4u?0:sector;
    volatile unsigned char *status=styleos_vblk_request+VB_STATUS_OFFSET;*status=255u;
    if(type==1u)copy(styleos_vblk_request+VB_DATA_OFFSET,data,VB_SECTOR_BYTES);
    struct vb_descriptor *desc=(struct vb_descriptor *)(void *)styleos_vblk_queue;
    desc[0]=(struct vb_descriptor){(uintptr_t)header,16u,DESC_NEXT,type==4u?2u:1u};
    desc[1]=(struct vb_descriptor){(uintptr_t)styleos_vblk_request+VB_DATA_OFFSET,
        VB_SECTOR_BYTES,(uint16_t)(DESC_NEXT|(type==0u?DESC_WRITE:0u)),2u};
    desc[2]=(struct vb_descriptor){(uintptr_t)status,1u,DESC_WRITE,0u};
    *avail_word(4u+2u*(s->avail%VB_QUEUE_SIZE))=0;
    barrier();++s->avail;*avail_word(2)=s->avail;
    ++s->requests;complete_barrier();wr(0x50,0u);
    unsigned n;
    for(n=0;n<VB_POLL_LIMIT;++n){
        if(s->bus.poll)s->bus.poll(s->bus.context);
        if(!healthy(15u))return fatal(VB_STATE);
        if(*used_word(2)!=s->used)break;
    }
    if(n==VB_POLL_LIMIT)return fatal(VB_TIMEOUT);
    barrier();
    if(*used_word(2)!=(uint16_t)(s->used+1u))return fatal(VB_USED);
    volatile struct vb_used_element *used=(volatile struct vb_used_element *)(void *)
        (styleos_vblk_queue+VB_USED_OFFSET+4u+8u*(s->used%VB_QUEUE_SIZE));
    if(used->id!=0u || used->length!=(type==0u?513u:1u))return fatal(VB_USED);
    if(*status!=0u)return fatal(VB_STATUS);
    if(rd(0xfc)!=s->generation)return fatal(VB_CONFIG);
    ++s->used;
    uint32_t irq=rd(0x60);if(irq)wr(0x64,irq);
    if(type==0u){copy(data,styleos_vblk_request+VB_DATA_OFFSET,VB_SECTOR_BYTES);++s->reads;}
    else if(type==1u)++s->writes;
    else ++s->flushes;
    s->busy=0;return VB_OK;
}
enum vb_error vb_read(uint64_t sector,void *data){return request(0u,sector,data);}
enum vb_error vb_write(uint64_t sector,const void *data){return request(1u,sector,(void *)data);}
enum vb_error vb_flush(void){return request(4u,0u,0);}
uint64_t vb_capacity(void){return CTL->ready?CTL->capacity:0;}
uint64_t vb_requests(void){return CTL->magic==VB_CTL_MAGIC?CTL->requests:0;}
uint64_t vb_reads(void){return CTL->magic==VB_CTL_MAGIC?CTL->reads:0;}
uint64_t vb_writes(void){return CTL->magic==VB_CTL_MAGIC?CTL->writes:0;}
uint64_t vb_flushes(void){return CTL->magic==VB_CTL_MAGIC?CTL->flushes:0;}
uint64_t vb_features(void){return CTL->ready?CTL->features:0;}
uint64_t vb_offered_features(void){return CTL->ready?CTL->offered:0;}
uint64_t vb_mmio_base(void){return CTL->ready?CTL->mmio:0;}
unsigned vb_queue_size(void){return CTL->ready?VB_QUEUE_SIZE:0;}
const char *vb_error_name(enum vb_error e)
{
    static const char *const names[]={"ok","argument","platform","memory","device","version",
        "features","read-only","queue","reset","capacity","range","limit","state",
        "timeout","used","status","config"};
    return (unsigned)e<sizeof names/sizeof names[0]?names[e]:"unknown";
}
#ifdef STYLEOS_VBLK_HOST_TEST
enum vb_error vb_model_open(const struct vb_model_bus *bus)
{
    if(!bus || !bus->read || !bus->write)return VB_ARGUMENT;
    const struct vb_bus b={bus->context,bus->read,bus->write,bus->poll};
    return initialize(&b,MMIO_FIRST,(struct boot_range){0,UINT64_MAX});
}
void vb_model_request_count(uint64_t count){CTL->requests=count;}
#endif
