#include <styleos/user_root_846.h>
#include <styleos/native_app_image_75.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
_Static_assert(NAI75_CODE_PAGES==8&&NAI75_DATA_PAGES==16&&NAI75_STACK_PAGES==4,"extent policy");
static _Alignas(4096) uint64_t st[PG_TABLE_PAGES][512],ta[PG_TABLE_PAGES][512],tb[PG_TABLE_PAGES][512];
static _Alignas(4096) uint8_t mem[S844_BYTES];
static struct s844_arena arena;
static struct s842_root safe;
static struct s846_root a,b;
static struct s844_lease leases[S846_ROOT_COUNT];
static struct s846_extent sizes[S846_ROOT_COUNT];
static unsigned checks;
#define CHECK(x) do{assert(x);++checks;}while(0)
#define P UINT64_C(4096)
#define NX (UINT64_C(1)<<63)
static uint64_t *find(struct s846_root *r,uint64_t va){
 size_t t=0;
 for(unsigned i=0;i<3;i++){
  uint64_t e=r->plan.tables[t][(va>>(39-9*i))&511u];
  if(!(e&1))return 0;
  t=(size_t)(((e&UINT64_C(0x000ffffffffff000))-r->plan.tables_pa)/P);
  CHECK(t<r->built.used);
 }
 return &r->plan.tables[t][(va>>12)&511];
}
static bool absent(struct s846_root *r,uint64_t va){uint64_t *e=find(r,va);return !e||!*e;}
static void verify(struct s846_root *r,unsigned id){
 const uint64_t va[]={S846_CODE,S846_DATA,S846_STACK};
 const unsigned n[]={sizes[id].code,sizes[id].data,sizes[id].stack};
 uint64_t pa=leases[id].pa;
 for(unsigned k=0;k<3;k++){
  CHECK(absent(r,va[k]-P));CHECK(absent(r,va[k]+n[k]*P));
  for(unsigned i=0;i<n[k];++i,pa+=P){
   uint64_t *e=find(r,va[k]+i*P);
   CHECK(e&&*e==(pa|(k?7|NX:5)));
  }
 }
 for(unsigned i=0;i<S844_PAGES;++i)CHECK(absent(r,arena.base+i*P));
 CHECK((*find(r,0x200000)&4)==0);CHECK((*find(r,0xfee00000)&4)==0);
}
int main(void){
 uint64_t pa=(uintptr_t)mem;
 CHECK(s844_init(&arena,pa));
 sizes[0]=(struct s846_extent){8,16,4};
 sizes[1]=(struct s846_extent){1,1,1};
 CHECK(s844_acquire(&arena,S846_OWNER_A,28,&leases[0]));
 CHECK(s844_acquire(&arena,S846_OWNER_B,3,&leases[1]));
 struct pg_plan p={.arch=PG_X86,.pa_bits=48,.tables=st,.tables_pa=0x200000,
  .capacity=PG_TABLE_PAGES,.count=3,.probe=0x400000001000,
  .maps={{0x200000,0x200000,3*PG_TABLE_BYTES+3*4096,PG_WRITE},
         {0x400000,0x400000,4096,PG_EXEC},{pa,pa,S844_BYTES,PG_WRITE}}};
 struct boot_memory_map m={.ram={{0x100000,0x8000000}},.nr_ram=1};
 struct s842_cache c={.pat=0x0007040600070406,.cap=1,.def=0x806,.count=1,.bits=48,
  .pairs={{0x80000000,0xffff80000800}}};
 struct s846_guard guard={{0x260000,0x261000,0x262000}};
 CHECK(s842_build(&p,&m,&c,0xfee00000,0xfec00000,&safe));
 CHECK(s875_root_build(&safe,&c,&arena,leases,sizes,0,&guard,ta,0x220000,&a));
 CHECK(s875_root_build(&safe,&c,&arena,leases,sizes,1,&guard,tb,0x240000,&b));
 CHECK(s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));
 CHECK(s875_root_audit(&safe,&c,&arena,leases,sizes,1,&guard,&b));
 verify(&a,0);verify(&b,1);
 for(unsigned i=0;i<3;i++){
  uint64_t va=S846_IST+3*i*P;
  CHECK(*find(&a,va)==(guard.pa[i]|3|NX));CHECK(absent(&a,va-P));CHECK(absent(&a,va+P));
 }
 /* Late-page permission, foreign PA, missing leaf, guard and ancestor checks. */
 uint64_t *e=find(&a,S846_CODE+7*P),old=*e;
 *e|=2;CHECK(!s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));*e=old;
 e=find(&a,S846_DATA+15*P);old=*e;
 *e&=~NX;CHECK(!s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));*e=old;
 *e=leases[1].pa|7|NX;CHECK(!s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));*e=old;
 e=find(&a,S846_STACK+3*P);old=*e;*e=0;
 CHECK(!s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));*e=old;
 e=find(&a,S846_STACK+4*P);CHECK(e&&!*e);*e=leases[0].pa|7|NX;
 CHECK(!s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));*e=0;
 struct s846_extent prior=sizes[0];sizes[0].data--;
 CHECK(!s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));
 sizes[0]=prior;sizes[2].code=1;
 CHECK(!s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));sizes[2].code=0;
 CHECK(!s865_root_audit(&safe,&c,&arena,leases,0,&guard,&a));
 CHECK(s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));
 /* Retirement: old root cannot pass even if its tables remain intact. */
 CHECK(s844_release(&arena,leases[0].owner,leases[0]));
 CHECK(!s875_root_audit(&safe,&c,&arena,leases,sizes,0,&guard,&a));
 memset(&leases[0],0,sizeof leases[0]);memset(&sizes[0],0,sizeof sizes[0]);
 CHECK(s875_root_audit(&safe,&c,&arena,leases,sizes,1,&guard,&b));
 CHECK(s844_release(&arena,leases[1].owner,leases[1]));
 unsigned used=99;CHECK(s844_count(&arena,&used)&&!used);
 printf("Native app root 75: %u checks PASS (detached PTE model, no activation)\n",checks);
}
