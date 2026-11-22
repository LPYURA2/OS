#include <styleos/production_closure_792.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct pc792_item sample(enum pc792_role r)
{
    struct pc792_item x={.role=r,.kind=PC792_RAM,.cache=PC792_WB,
        .evidence=PC792_REPORTED,.start=UINT64_C(0x100000)+(uint64_t)r*UINT64_C(0x20000),
        .length=UINT64_C(4096)};
    if(r==PC792_IMAGE_RX)x.executable=true;
    if(r==PC792_IMAGE_RW || r==PC792_BOOT_STACK || r==PC792_IST_STACKS ||
       r==PC792_DESCRIPTORS || r==PC792_PAGE_TABLES)x.writable=true;
    if(r==PC792_GOP_FRAMEBUFFER){x.kind=PC792_FRAMEBUFFER;x.cache=PC792_WC;x.writable=true;}
    if(r==PC792_DEVICE_MMIO){x.kind=PC792_MMIO;x.cache=PC792_UC;x.writable=true;}
    if(r==PC792_IO_PORTS){x.kind=PC792_IOPORT;x.cache=PC792_CACHE_UNKNOWN;
       x.start=UINT64_C(0x3f8);x.length=8;x.writable=true;}
    if(r==PC792_INTERRUPT_STATE || r==PC792_FIRMWARE_POLICY){
       x.kind=PC792_ASSERTION;x.cache=PC792_CACHE_UNKNOWN;x.start=x.length=0;}
    return x;
}
int main(void)
{
    struct pc792_item a[PC792_ROLE_COUNT];
    struct pc792_report r;
    assert(pc792_inventory(NULL,0,&r)==PC792_OK);
    assert(r.missing_mask==PC792_REQUIRED_MASK && !r.activation_permitted);
    assert(pc792_inventory(NULL,1,&r)==PC792_ARGUMENT);
    assert(pc792_inventory(a,PC792_MAX_ITEMS+1,&r)==PC792_ARGUMENT);
    assert(pc792_inventory(NULL,0,NULL)==PC792_ARGUMENT);
    for(unsigned i=0;i<PC792_ROLE_COUNT;++i){
       a[i]=sample((enum pc792_role)i);
       assert(strcmp(pc792_role_name((enum pc792_role)i),"invalid")!=0);
    }
    assert(!strcmp(pc792_role_name(PC792_ROLE_COUNT),"invalid"));
    assert(pc792_inventory(a,PC792_ROLE_COUNT,&r)==PC792_OK);
    assert(!r.missing_mask && r.unverified_mask==PC792_REQUIRED_MASK &&
           !r.attested_mask && !r.activation_permitted);
    for(unsigned i=0;i<PC792_ROLE_COUNT;++i)a[i].evidence=PC792_ATTESTED;
    assert(pc792_inventory(a,PC792_ROLE_COUNT,&r)==PC792_OK &&
           !r.unverified_mask && r.attested_mask==PC792_REQUIRED_MASK &&
           !r.activation_permitted); /* Synthetic full evidence NEVER permits CR3. */
    struct pc792_item old;
#define NEG(index, mutate, expected) do{old=a[index];mutate;  assert(pc792_inventory(a,PC792_ROLE_COUNT,&r)==expected);  assert(!r.activation_permitted);a[index]=old;}while(0)
    NEG(PC792_IMAGE_RX,a[PC792_IMAGE_RX].writable=true,PC792_RIGHTS);
    NEG(PC792_IMAGE_RW,a[PC792_IMAGE_RW].executable=true,PC792_RIGHTS);
    NEG(PC792_BOOT_STACK,a[PC792_BOOT_STACK].user=true,PC792_RIGHTS);
    NEG(PC792_IMAGE_RO,a[PC792_IMAGE_RO].cache=PC792_CACHE_UNKNOWN,PC792_CACHE_POLICY);
    NEG(PC792_GOP_FRAMEBUFFER,a[PC792_GOP_FRAMEBUFFER].cache=PC792_WB,PC792_CACHE_POLICY);
    NEG(PC792_DEVICE_MMIO,a[PC792_DEVICE_MMIO].kind=PC792_RAM,PC792_RANGE);
    NEG(PC792_IO_PORTS,a[PC792_IO_PORTS].start=65535,PC792_RANGE);
    NEG(PC792_IO_PORTS,a[PC792_IO_PORTS].cache=PC792_UC,PC792_RANGE);
    NEG(PC792_IST_STACKS,a[PC792_IST_STACKS].start+=1,PC792_RANGE);
    NEG(PC792_PAGE_TABLES,a[PC792_PAGE_TABLES].length=4097,PC792_RANGE);
    NEG(PC792_BOOT_MAP,a[PC792_BOOT_MAP].start=a[PC792_IMAGE_RX].start,PC792_OVERLAP);
    NEG(PC792_FIRMWARE_POLICY,a[PC792_FIRMWARE_POLICY].length=1,PC792_RANGE);
    NEG(PC792_GOP_FRAMEBUFFER,a[PC792_GOP_FRAMEBUFFER].role=PC792_IMAGE_RX,PC792_DUPLICATE);
    NEG(PC792_DESCRIPTORS,a[PC792_DESCRIPTORS].evidence=99,PC792_ARGUMENT);
    NEG(PC792_IMAGE_RX,a[PC792_IMAGE_RX].start=UINT64_MAX-4095,PC792_RANGE);
    assert(pc792_inventory(a,PC792_ROLE_COUNT-1,&r)==PC792_OK);
    assert(r.missing_mask==(UINT32_C(1)<<PC792_FIRMWARE_POLICY) &&
           !r.activation_permitted);
    puts("STYLEOS 792 PRODUCTION CLOSURE PREFLIGHT PASS (not CR3-ready)");
    return 0;
}
