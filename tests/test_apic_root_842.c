#include <styleos/apic_root_842.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint64_t tables[PG_TABLE_PAGES][512];
int main(void){
 const uint64_t lapic=0xfee00000,io=0xfec00000;
 struct pg_plan p={.arch=PG_X86,.pa_bits=48,.tables=tables,.tables_pa=0x200000,.capacity=PG_TABLE_PAGES,.count=2,.probe=0x400000001000,.maps={{0x200000,0x200000,PG_TABLE_BYTES,PG_WRITE},{0x400000,0x400000,4096,PG_EXEC}}};
 struct boot_memory_map m={.ram={{0x100000,0x8000000}},.nr_ram=1};
 struct s842_cache c={.pat=0x0007040600070406,.cap=1,.def=0x806,.count=1,.bits=48,.pairs={{0x80000000,0xffff80000800}}};
 struct s842_root r;unsigned n=0;
 #define CHECK(x) do{assert(x);n++;}while(0)
 CHECK(s842_build(&p,&m,&c,lapic,io,&r));CHECK(s842_audit(&r,&c));
 CHECK(!s842_build(&p,&m,&c,lapic,lapic,&r)&&!r.ready);
 CHECK(!s842_build(&p,&m,&c,lapic+1,io,&r));
 struct s842_cache bad=c;bad.def=6;CHECK(!s842_build(&p,&m,&bad,lapic,io,&r));
 bad=c;bad.pat=0x0606060606060606;CHECK(!s842_build(&p,&m,&bad,lapic,io,&r));
 m.candidates[0]=(struct boot_range){io,io+4096};m.nr_candidates=1;CHECK(!s842_build(&p,&m,&c,lapic,io,&r));m.nr_candidates=0;
 m.ram[1]=(struct boot_range){lapic,lapic+4096};m.nr_ram=2;CHECK(!s842_build(&p,&m,&c,lapic,io,&r));m.nr_ram=1;
 struct pg_plan q=p;q.maps[1].flags=PG_EXEC|PG_WRITE;CHECK(!s842_build(&q,&m,&c,lapic,io,&r));q=p;q.maps[1].pa=io;CHECK(!s842_build(&q,&m,&c,lapic,io,&r));
 CHECK(s842_build(&p,&m,&c,lapic,io,&r));tables[0][1]=tables[0][0];CHECK(!s842_audit(&r,&c));
 CHECK(s842_build(&p,&m,&c,lapic,io,&r));tables[0][0]|=4;CHECK(!s842_audit(&r,&c));
 CHECK(s842_build(&p,&m,&c,lapic,io,&r));size_t found=0;for(size_t t=0;t<r.built.used;t++)for(size_t j=0;j<512;j++)if((tables[t][j]&0x000ffffffffff000)==lapic){tables[t][j]^=16;found++;}CHECK(found==1&&!s842_audit(&r,&c));
 q=p;q.maps[0].flags=0;CHECK(!s842_build(&q,&m,&c,lapic,io,&r));
 q=p;q.maps[1]=(struct pg_mapping){0x80000000,0x80000000,4096,PG_EXEC};CHECK(!s842_build(&q,&m,&c,lapic,io,&r));
 q=p;q.tables_pa=io;CHECK(!s842_build(&q,&m,&c,lapic,io,&r));
 q=p;q.maps[1]=(struct pg_mapping){0x400000,0x400000,UINT64_C(1)<<40,PG_EXEC};CHECK(!s842_build(&q,&m,&c,lapic,io,&r));
 CHECK(s842_build(&p,&m,&c,lapic,io,&r));r.plan.tables_pa+=1;CHECK(!s842_audit(&r,&c));
 CHECK(s842_build(&p,&m,&c,lapic,io,&r));r.plan.maps[1].flags=PG_WRITE;CHECK(!s842_audit(&r,&c));
 CHECK(s842_build(&p,&m,&c,lapic,io,&r));r.built.used--;CHECK(!s842_audit(&r,&c));
 printf("S842 detached root model: %u PASS (not guest evidence)\n",n);return 0;
}
