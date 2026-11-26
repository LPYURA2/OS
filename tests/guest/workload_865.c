/* CI workload provider, never a release application loader. */
#include "workload_865.h"
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
static uint8_t elf865[8192];
static bool stage_elf(unsigned id,uint8_t *page,struct nte_loaded *loaded){
 const uint32_t lba=3072+16*id;
 enum nte_status result=nte_load_x64_reader(read865,0,lba,4096,external865[id].bytes,
    external865[id].hash,elf865,sizeof elf865,page,P,loaded);
 for(size_t j=0;j<sizeof elf865;j++)elf865[j]=0;
 if(result!=NTE_OK)return false;
 uint8_t observed[32];nte_sha256(page,P,observed);
 for(unsigned j=0;j<32;j++)if(observed[j]!=external865[id].page_hash[j])return false;
 arch_puts("STYLEOS S865 EXTERNAL-ELF");field865("id",id);field865("lba",lba);field865("bytes",external865[id].bytes);field865("sectors",(external865[id].bytes+511)/512);arch_putc('\n');
 return true;
}
bool s865_fixture_stage(const struct s844_lease leases[4],struct s865_fixture_image out[4]){
 if(!leases||!out)return false;
 struct s865_fixture_image prepared[4];
 for(unsigned i=0;i<4;i++){
  if(!leases[i].pa||(leases[i].pa&(P-1))||leases[i].pa>(UINT64_C(1)<<47)-3*P||
     leases[i].pages!=3||leases[i].owner!=S846_OWNER_A+i||!leases[i].generation)return false;
  for(unsigned j=0;j<i;j++)if(leases[i].pa<leases[j].pa+3*P&&leases[j].pa<leases[i].pa+3*P)return false;
  prepared[i]=(struct s865_fixture_image){
   .image={.entry=S846_CODE,.code_bytes=external865[i].code_bytes,.memory_bytes=P},
   .loop_end=external865[i].loop,.terminal_pc=external865[i].term,.call_pc=external865[i].call};
 }
 for(unsigned i=0;i<4;i++){
  volatile unsigned char *dst=(void *)(uintptr_t)leases[i].pa;
  struct nte_loaded actual={0};
  if(!stage_elf(i,(uint8_t *)(uintptr_t)leases[i].pa,&actual)||
     actual.entry!=prepared[i].image.entry||actual.code_bytes!=prepared[i].image.code_bytes||
     actual.memory_bytes!=P)return false;
  prepared[i].image=actual;
  for(size_t j=P;j<3*P;j++)dst[j]=0;
  for(size_t j=P;j<3*P;j++)if(dst[j])return false;
 }
 for(unsigned i=0;i<4;i++)out[i]=prepared[i];
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
