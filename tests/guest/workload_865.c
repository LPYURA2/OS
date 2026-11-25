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
  for(size_t j=0;j<3*P;j++)dst[j]=j<len?start[i][j]:0;
  for(size_t j=0;j<3*P;j++)if(dst[j]!=(j<len?start[i][j]:0))return false;
 }
 for(unsigned i=0;i<4;i++)out[i]=prepared[i];
 return true;
}
