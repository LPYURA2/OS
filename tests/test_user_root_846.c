#include <styleos/user_root_846.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint64_t safe_tables[PG_TABLE_PAGES][512],ta[PG_TABLE_PAGES][512],tb[PG_TABLE_PAGES][512];
static _Alignas(4096) unsigned char arena_mem[S844_BYTES];
static struct s844_arena arena;
static struct s842_root safe;
static struct s846_root a,b;
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static uint64_t *find(struct s846_root *r,uint64_t va){size_t t=0;for(unsigned i=0;i<3;i++){uint64_t e=r->plan.tables[t][(va>>(39-9*i))&511u];if(!(e&1))return 0;t=(size_t)(((e&UINT64_C(0x000ffffffffff000))-r->plan.tables_pa)/4096);}return &r->plan.tables[t][(va>>12)&511];}
int main(void){
 uint64_t pa=(uintptr_t)arena_mem;
 struct s844_lease leases[2];CHECK(s844_init(&arena,pa));CHECK(s844_acquire(&arena,S846_OWNER_A,3,&leases[0]));CHECK(s844_acquire(&arena,S846_OWNER_B,3,&leases[1]));
 struct pg_plan p={.arch=PG_X86,.pa_bits=48,.tables=safe_tables,.tables_pa=0x200000,.capacity=PG_TABLE_PAGES,.count=3,.probe=0x400000001000,.maps={{0x200000,0x200000,3*PG_TABLE_BYTES+3*4096,PG_WRITE},{0x400000,0x400000,4096,PG_EXEC},{pa,pa,S844_BYTES,PG_WRITE}}};
 struct boot_memory_map m={.ram={{0x100000,0x8000000}},.nr_ram=1};
 struct s842_cache c={.pat=0x0007040600070406,.cap=1,.def=0x806,.count=1,.bits=48,.pairs={{0x80000000,0xffff80000800}}};
 CHECK(s842_build(&p,&m,&c,0xfee00000,0xfec00000,&safe));
 CHECK(s846_build(&safe,&c,&arena,leases,0,NULL,ta,0x220000,&a));CHECK(s846_build(&safe,&c,&arena,leases,1,NULL,tb,0x240000,&b));
 CHECK(s846_audit(&safe,&c,&arena,leases,0,NULL,&a));CHECK(s846_audit(&safe,&c,&arena,leases,1,NULL,&b));CHECK(s842_audit(&safe,&c));CHECK(s846_audit_pair(&safe,&c,&arena,leases,NULL,&a,&b));CHECK(!s846_audit_pair(&safe,&c,&arena,leases,NULL,&a,&a));CHECK(!s846_audit_pair(&safe,&c,&arena,leases,NULL,&b,&a));
 CHECK(*find(&a,S846_CODE)==(leases[0].pa|5));CHECK(*find(&b,S846_CODE)==(leases[1].pa|5));
 CHECK(*find(&a,S846_DATA)==((leases[0].pa+4096)|7|(UINT64_C(1)<<63)));CHECK(*find(&a,S846_STACK)==((leases[0].pa+8192)|7|(UINT64_C(1)<<63)));
 CHECK(!find(&a,pa)||!*find(&a,pa));CHECK(!find(&b,pa)||!*find(&b,pa));CHECK(!find(&a,S846_STACK-4096)||!*find(&a,S846_STACK-4096));
 CHECK((*find(&a,0x200000)&4)==0);CHECK((*find(&a,0xfee00000)&4)==0);
 uint64_t *x=find(&a,S846_CODE),old=*x;*x|=2;CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));*x=old;
 *x=leases[1].pa|5;CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));*x=old;
 *x|=0x60;CHECK(s846_audit(&safe,&c,&arena,leases,0,NULL,&a));*x=old;
 x=find(&a,0x200000);old=*x;*x|=4;CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));*x=old;
 x=&ta[0][0];old=*x;*x&=~UINT64_C(4);CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));*x=old;
 x=find(&a,S846_STACK);old=*x;*x=0;CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));*x=old;
 x=find(&a,S846_STACK+4096);CHECK(x&&!*x);*x=leases[0].pa|5;CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));*x=0;
 CHECK(!s846_audit(&safe,&c,&arena,leases,1,NULL,&a));CHECK(s846_audit(&safe,&c,&arena,leases,0,NULL,&a));
 struct s846_root empty={0};CHECK(!s846_build(&safe,&c,&arena,leases,0,NULL,safe_tables,0x220000,&empty));CHECK(!s846_build(&safe,&c,&arena,leases,0,NULL,ta,0x200000,&empty));CHECK(!empty.ready);
 CHECK(!s846_build(&safe,&c,&arena,leases,0,NULL,ta,0x220000,&a));
 CHECK(!s846_build(&safe,&c,&arena,leases,2,NULL,ta,0x220000,&empty));
 CHECK(!s846_build(&safe,&c,&arena,leases,0,NULL,ta,0x600000,&empty));
 x=find(&a,0xfee00000);old=*x;*x^=16;CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));*x=old;
 x=find(&a,S846_DATA);old=*x;*x&=~(UINT64_C(1)<<63);CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));*x=old;
 size_t used=a.built.used;a.built.used++;CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));a.built.used=used;
 struct s844_lease swapped[2]={leases[1],leases[0]};CHECK(!s846_audit(&safe,&c,&arena,swapped,0,NULL,&a));
 p.maps[p.count++]=(struct pg_mapping){S846_STACK-4096,S846_STACK-4096,4096,PG_WRITE};
 CHECK(s842_build(&p,&m,&c,0xfee00000,0xfec00000,&safe));CHECK(!s846_build(&safe,&c,&arena,leases,0,NULL,ta,0x220000,&empty));
 p.count--;CHECK(s842_build(&p,&m,&c,0xfee00000,0xfec00000,&safe));CHECK(s846_audit(&safe,&c,&arena,leases,0,NULL,&a));
 struct s846_guard g={{0x260000,0x261000,0x262000}};
 memset(&a,0,sizeof a);memset(&b,0,sizeof b);
 CHECK(s846_build(&safe,&c,&arena,leases,0,&g,ta,0x220000,&a));
 CHECK(s846_build(&safe,&c,&arena,leases,1,&g,tb,0x240000,&b));
 CHECK(s846_audit_pair(&safe,&c,&arena,leases,&g,&a,&b));
 for(unsigned i=0;i<3;i++){
  uint64_t va=S846_IST+3*i*4096;uint64_t *entry=find(&a,va);
  CHECK(entry&&*entry==(g.pa[i]|3|(UINT64_C(1)<<63)));
  CHECK(!find(&a,va-4096)||!*find(&a,va-4096));CHECK(!find(&a,va+4096)||!*find(&a,va+4096));
  *entry|=4;CHECK(!s846_audit(&safe,&c,&arena,leases,0,&g,&a));*entry&=~UINT64_C(4);
 }
 CHECK(!s846_audit_pair(&safe,&c,&arena,leases,NULL,&a,&b));
 struct s846_guard bad={{g.pa[0],g.pa[0],g.pa[2]}};
 CHECK(!s846_audit(&safe,&c,&arena,leases,0,&bad,&a));
 CHECK(!s846_audit(&safe,&c,&arena,leases,0,&g,&b));
 CHECK(s844_release(&arena,S846_OWNER_A,leases[0]));CHECK(!s846_audit(&safe,&c,&arena,leases,0,NULL,&a));CHECK(!s846_audit(&safe,&c,&arena,leases,1,NULL,&b));CHECK(!s846_audit_pair(&safe,&c,&arena,leases,NULL,&a,&b));
 printf("S846 detached user roots: %u PASS (model only)\n",checks);
}
