#include <styleos/user_root_846.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
_Static_assert(S846_ROOT_COUNT==4,"wide test only");
static _Alignas(4096) uint64_t safe_tables[PG_TABLE_PAGES][512],tables[4][PG_TABLE_PAGES][512];
static _Alignas(4096) unsigned char arena_mem[S844_BYTES];
static struct s844_arena arena;
static struct s842_root safe;
static struct s846_root roots[4];
static unsigned checks;
#define C(x) do{assert(x);++checks;}while(0)
static uint64_t *find(struct s846_root *r,uint64_t va){
 size_t t=0;for(unsigned i=0;i<3;i++){uint64_t e=r->plan.tables[t][(va>>(39-9*i))&511u];if(!(e&1))return 0;t=(size_t)(((e&UINT64_C(0x000ffffffffff000))-r->plan.tables_pa)/4096);}
 return &r->plan.tables[t][(va>>12)&511];
}
int main(void){
 uint64_t pa=(uintptr_t)arena_mem;
 struct s844_lease l[4];C(s844_init(&arena,pa));
 for(unsigned i=0;i<4;i++)C(s844_acquire(&arena,S846_OWNER_A+i,3,&l[i]));
 unsigned used;C(s844_count(&arena,&used)&&used==12);
 struct pg_plan p={.arch=PG_X86,.pa_bits=48,.tables=safe_tables,.tables_pa=0x200000,.capacity=PG_TABLE_PAGES,.count=3,.probe=0x400000001000,.maps={{0x200000,0x200000,5*PG_TABLE_BYTES+3*4096,PG_WRITE},{0x400000,0x400000,4096,PG_EXEC},{pa,pa,S844_BYTES,PG_WRITE}}};
 struct boot_memory_map m={.ram={{0x100000,0x8000000}},.nr_ram=1};
 struct s842_cache c={.pat=0x0007040600070406,.cap=1,.def=0x806,.count=1,.bits=48,.pairs={{0x80000000,0xffff80000800}}};
 struct s846_guard g={{0x200000+5*PG_TABLE_BYTES,0x201000+5*PG_TABLE_BYTES,0x202000+5*PG_TABLE_BYTES}};
 C(s842_build(&p,&m,&c,0xfee00000,0xfec00000,&safe));
 for(unsigned i=0;i<4;i++)C(s846_build(&safe,&c,&arena,l,i,&g,tables[i],0x200000+(i+1)*PG_TABLE_BYTES,&roots[i]));
 C(s846_audit_set(&safe,&c,&arena,l,&g,roots));
 const uint64_t va[]={S846_CODE,S846_DATA,S846_STACK};
 for(unsigned i=0;i<4;i++){
  for(unsigned j=0;j<3;j++){
   uint64_t *e=find(&roots[i],va[j]),old=*e;
   C(old==((l[i].pa+j*4096)|(j?UINT64_C(0x8000000000000007):5)));
   for(unsigned k=0;k<4;k++)if(k!=i){
    *e=(l[k].pa+j*4096)|(old&UINT64_C(0x8000000000000fff));
    C(!s846_audit_set(&safe,&c,&arena,l,&g,roots));*e=old;
   }
   *e=old^2;C(!s846_audit_set(&safe,&c,&arena,l,&g,roots));*e=old;
  }
  C(!find(&roots[i],pa)||!*find(&roots[i],pa));
  C(!find(&roots[i],S846_STACK-4096)||!*find(&roots[i],S846_STACK-4096));
  C((*find(&roots[i],S846_IST)&4)==0);
 }
 C(s846_audit_set(&safe,&c,&arena,l,&g,roots));
 struct s846_root saved=roots[3];roots[3]=roots[0];C(!s846_audit_set(&safe,&c,&arena,l,&g,roots));roots[3]=saved;
 struct s844_lease old=l[2];C(s844_release(&arena,old.owner,old));C(!s846_audit_set(&safe,&c,&arena,l,&g,roots));
 C(s844_acquire(&arena,S846_OWNER_A+2,3,&l[2])&&l[2].pa==old.pa&&l[2].generation!=old.generation);
 C(!s844_release(&arena,old.owner,old));
 /* Replacing a lease requires re-establishing its root. Existing root
  * descriptors have no generation field; never pass an old root as proof
  * of fresh allocation. The future hardware adapter must bind generation. */
 memset(&roots[2],0,sizeof roots[2]);
 C(s846_build(&safe,&c,&arena,l,2,&g,tables[2],0x200000+3*PG_TABLE_BYTES,&roots[2]));
 C(s846_audit_set(&safe,&c,&arena,l,&g,roots));
 memset(roots,0,sizeof roots);memset(tables,0,sizeof tables);
 for(unsigned i=0;i<4;i++)C(s844_release(&arena,l[i].owner,l[i]));
 C(s844_count(&arena,&used)&&used==0);
 for(unsigned i=0;i<sizeof arena_mem;i++)assert(!arena_mem[i]);
 printf("S864 four detached root contracts HOST ONLY: %u checks PASS\n",checks);
}
