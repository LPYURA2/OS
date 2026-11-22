#include <styleos/production_closure_792.h>

#define PAGE UINT64_C(4096)
#define PHYS_LIMIT (UINT64_C(1)<<52)
static bool assertion(enum pc792_role r)
{ return r==PC792_INTERRUPT_STATE || r==PC792_FIRMWARE_POLICY; }
static bool is_write(enum pc792_role r)
{ return r==PC792_IMAGE_RW || r==PC792_BOOT_STACK || r==PC792_IST_STACKS ||
         r==PC792_DESCRIPTORS || r==PC792_PAGE_TABLES; }
static bool is_exec(enum pc792_role r)
{ return r==PC792_IMAGE_RX; }
static bool span_ok(uint64_t start,uint64_t n,uint64_t limit)
{ return start<limit && n!=0 && n<=limit-start; }
static bool memory_kind(enum pc792_kind k)
{ return k==PC792_RAM || k==PC792_FRAMEBUFFER || k==PC792_MMIO; }
const char *pc792_role_name(enum pc792_role role)
{
    static const char *const names[PC792_ROLE_COUNT]={
        "pe-rx","pe-ro","pe-rw","boot-stack","ist-stacks",
        "descriptors","page-tables","boot-memory-map","gop-framebuffer",
        "device-mmio","io-ports","interrupt-state","firmware-policy"};
    return (unsigned)role<PC792_ROLE_COUNT?names[role]:"invalid";
}
enum pc792_status pc792_inventory(const struct pc792_item *items,
                                  size_t count,struct pc792_report *out)
{
    if(out)*out=(struct pc792_report){.missing_mask=PC792_REQUIRED_MASK,
                                      .activation_permitted=false};
    if(!out || (!items&&count) || count>PC792_MAX_ITEMS)return PC792_ARGUMENT;
    for(size_t i=0;i<count;++i){
        const struct pc792_item *a=&items[i];
        if((unsigned)a->role>=PC792_ROLE_COUNT ||
           (unsigned)a->evidence>PC792_ATTESTED)return PC792_ARGUMENT;
        const uint32_t bit=UINT32_C(1)<<a->role;
        if(out->observed_mask&bit){++out->conflicts;return PC792_DUPLICATE;}
        bool isassert=assertion(a->role);
        if(isassert){
            if(a->kind!=PC792_ASSERTION || a->start || a->length ||
               a->cache!=PC792_CACHE_UNKNOWN || a->writable ||
               a->executable || a->user)return PC792_RANGE;
        }else if(a->role==PC792_IO_PORTS){
            if(a->kind!=PC792_IOPORT ||
               !span_ok(a->start,a->length,UINT64_C(65536)) ||
               a->cache!=PC792_CACHE_UNKNOWN)return PC792_RANGE;
        }else{
            if(!memory_kind(a->kind) ||
               !span_ok(a->start,a->length,PHYS_LIMIT) ||
               (a->start&(PAGE-1)) || (a->length&(PAGE-1)))return PC792_RANGE;
            if((a->role==PC792_GOP_FRAMEBUFFER && a->kind!=PC792_FRAMEBUFFER) ||
               (a->role==PC792_DEVICE_MMIO && a->kind!=PC792_MMIO) ||
               (a->role!=PC792_GOP_FRAMEBUFFER &&
                a->role!=PC792_DEVICE_MMIO && a->kind!=PC792_RAM))return PC792_RANGE;
            if((a->kind==PC792_RAM && a->cache!=PC792_WB) ||
               (a->kind==PC792_FRAMEBUFFER &&
                 a->cache!=PC792_WC && a->cache!=PC792_UC) ||
               (a->kind==PC792_MMIO && a->cache!=PC792_UC))return PC792_CACHE_POLICY;
        }
        if(a->user || a->executable!=is_exec(a->role) ||
           (a->writable!=is_write(a->role) &&
           a->role!=PC792_GOP_FRAMEBUFFER &&
           a->role!=PC792_DEVICE_MMIO && a->role!=PC792_IO_PORTS))
            return PC792_RIGHTS;
        if((a->role==PC792_GOP_FRAMEBUFFER || a->role==PC792_DEVICE_MMIO ||
            a->role==PC792_IO_PORTS) && (a->executable || !a->writable))
            return PC792_RIGHTS;
        for(size_t j=0;j<i;++j){
            const struct pc792_item *b=&items[j];
            if(memory_kind(a->kind) && memory_kind(b->kind) &&
               a->start<b->start+b->length && b->start<a->start+a->length){
                ++out->conflicts;return PC792_OVERLAP;
            }
        }
        out->observed_mask |= bit;
        out->missing_mask &= ~bit;
        if(a->evidence==PC792_ATTESTED)out->attested_mask |= bit;
        else out->unverified_mask |= bit;
    }
    out->activation_permitted=false;
    return PC792_OK;
}
