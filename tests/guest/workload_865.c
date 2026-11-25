/* CI workload provider, never a release application loader. */
#include "workload_865.h"
#if !defined(STYLEOS_INTEGRATED_RUNTIME_864) || !defined(STYLEOS_TEST_FOUR_GUEST_864)
#error test provider requires explicit integrated four-task fixture
#endif
#define P UINT64_C(4096)
_Static_assert(S846_ROOT_COUNT==4,"four workload images");
extern const unsigned char s848_a_start[],s848_a_loop_end[],s848_a_fault[],s848_a_end[],s848_a_yield[];
extern const unsigned char s848_b_start[],s848_b_loop_end[],s848_b_exit[],s848_b_end[],s848_b_yield[];
extern const unsigned char s848_c_start[],s848_c_loop_end[],s848_c_fault[],s848_c_end[],s848_c_yield[];
extern const unsigned char s848_d_start[],s848_d_loop_end[],s848_d_fault[],s848_d_end[],s848_d_yield[];
/* Package construction is deliberately confined to the test provider.
 * The kernel's loader alone validates ELF and writes the code page. */
static uint8_t elf865[8192];
static void putle(unsigned off,uint64_t value,unsigned bytes){
 for(unsigned i=0;i<bytes;i++)elf865[off+i]=(uint8_t)(value>>(8*i));
}
static bool stage_elf(const uint8_t *code,size_t len,uint8_t *page,struct nte_loaded *loaded){
 for(size_t j=0;j<sizeof elf865;j++)elf865[j]=0;
 elf865[0]=127;elf865[1]='E';elf865[2]='L';elf865[3]='F';
 elf865[4]=2;elf865[5]=1;elf865[6]=1;
 putle(16,2,2);putle(18,62,2);putle(20,1,4);putle(24,S846_CODE,8);
 putle(32,64,8);putle(52,64,2);putle(54,56,2);putle(56,1,2);
 putle(64,1,4);putle(68,5,4);putle(72,P,8);putle(80,S846_CODE,8);
 putle(88,S846_CODE,8);putle(96,len,8);putle(104,P,8);putle(112,P,8);
 for(size_t j=0;j<len;j++)elf865[P+j]=code[j];
 uint8_t hash[32];nte_sha256(elf865,P+len,hash);
 enum nte_status result=nte_load_x64_image(elf865,P+len,hash,page,P,loaded);
 for(size_t j=0;j<sizeof elf865;j++)elf865[j]=0;
 return result==NTE_OK;
}
bool s865_fixture_stage(const struct s844_lease leases[4],struct s865_fixture_image out[4]){
 const unsigned char *start[4]={s848_a_start,s848_b_start,s848_c_start,s848_d_start};
 const unsigned char *loop[4]={s848_a_loop_end,s848_b_loop_end,s848_c_loop_end,s848_d_loop_end};
 const unsigned char *end[4]={s848_a_end,s848_b_end,s848_c_end,s848_d_end};
 const unsigned char *term[4]={s848_a_fault,s848_b_exit,s848_c_fault,s848_d_fault};
 const unsigned char *calls[4]={s848_a_yield,s848_b_yield,s848_c_yield,s848_d_yield};
 if(!leases||!out)return false;
 struct s865_fixture_image prepared[4];
 for(unsigned i=0;i<4;i++){
  uintptr_t lo=(uintptr_t)start[i],hi=(uintptr_t)end[i],mid=(uintptr_t)loop[i],last=(uintptr_t)term[i],call=(uintptr_t)calls[i];
  if(lo>=mid||mid>last||last>=hi||hi-lo>P||call<=lo||call>=mid||
     !leases[i].pa||(leases[i].pa&(P-1))||leases[i].pa>(UINT64_C(1)<<47)-3*P||
     leases[i].pages!=3||leases[i].owner!=S846_OWNER_A+i||!leases[i].generation)return false;
  for(unsigned j=0;j<i;j++)if(leases[i].pa<leases[j].pa+3*P&&leases[j].pa<leases[i].pa+3*P)return false;
  prepared[i]=(struct s865_fixture_image){
   .image={.entry=S846_CODE,.code_bytes=(uint32_t)(hi-lo),.memory_bytes=P},
   .loop_end=S846_CODE+mid-lo,.terminal_pc=S846_CODE+last-lo,.call_pc=S846_CODE+call-lo};
 }
 for(unsigned i=0;i<4;i++){
  volatile unsigned char *dst=(void *)(uintptr_t)leases[i].pa;
  size_t len=prepared[i].image.code_bytes;
  struct nte_loaded actual={0};
  if(!stage_elf(start[i],len,(uint8_t *)(uintptr_t)leases[i].pa,&actual)||
     actual.entry!=prepared[i].image.entry||actual.code_bytes!=len||
     actual.memory_bytes!=P)return false;
  prepared[i].image=actual;
  for(size_t j=P;j<3*P;j++)dst[j]=0;
  for(size_t j=0;j<3*P;j++)if(dst[j]!=(j<len?start[i][j]:0))return false;
 }
 for(unsigned i=0;i<4;i++)out[i]=prepared[i];
 return true;
}
