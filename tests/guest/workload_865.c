/* CI workload provider, never a release application loader. */
#include "workload_865.h"
#include <styleos/native_image_slot_865.h>
#include <styleos/arch.h>
extern bool s863_disk_read(uint32_t,uint8_t[512]);
static bool read865(void *opaque,uint32_t lba,uint8_t out[512]){(void)opaque;return s863_disk_read(lba,out);}
static void field865(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_puts("=0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)i)&15]);}
#if !defined(STYLEOS_INTEGRATED_RUNTIME_864) || !defined(STYLEOS_TEST_FOUR_GUEST_864)
#error test provider requires explicit integrated four-task fixture
#endif
#define P UINT64_C(4096)
_Static_assert(S846_ROOT_COUNT==4,"four workload images");
#include "external_workload_865.h"
bool s865_fixture_source(unsigned id,struct s865_image_source *out){
 if(!out||id>=S846_ROOT_COUNT)return false;
 struct s865_image_source source={.read=read865,.lba=3072+16*id,.sectors=4096,.bytes=external865[id].bytes};
 for(unsigned j=0;j<32;j++)source.sha256[j]=external865[id].hash[j];
 *out=source;return true;
}
bool s865_fixture_observe(unsigned id,struct s844_lease lease,const struct nte_loaded *actual,struct s865_fixture_image *out){
 if(!out||!actual||id>=S846_ROOT_COUNT||lease.owner!=S846_OWNER_A+id||
    actual->entry!=S846_CODE||actual->code_bytes!=external865[id].code_bytes||actual->memory_bytes!=P)return false;
 uint8_t observed[32];nte_sha256((const void *)(uintptr_t)lease.pa,P,observed);
 for(unsigned j=0;j<32;j++)if(observed[j]!=external865[id].page_hash[j])return false;
 arch_puts("STYLEOS S865 EXTERNAL-ELF");field865("id",id);field865("lba",3072+16*id);field865("bytes",external865[id].bytes);field865("sectors",(external865[id].bytes+511)/512);arch_putc('\n');
 *out=(struct s865_fixture_image){*actual,external865[id].loop,external865[id].term,external865[id].call};
 return true;
}

/* Explicit CI workload request; never part of the scheduler's policy. */
bool s865_fixture_request_stop(unsigned id,uint64_t ticks,const struct s844_lease *lease,unsigned budget){
 if(!lease||id>=4||lease->owner!=S846_OWNER_A+id||lease->pages!=3||!lease->generation||!lease->pa||(lease->pa&(P-1))||lease->pa>(UINT64_C(1)<<47)-3*P)return false;
 if(ticks==budget)*(volatile uint64_t *)(uintptr_t)(lease->pa+P+8)=1;
 return true;
}

/* One actual pending IRQ per batch, induced only by the CI workload. */
bool s865_fixture_force_timer(unsigned id,uint64_t vector,uint64_t call,unsigned seen){return id==0&&vector==128&&call==1&&!seen;}
