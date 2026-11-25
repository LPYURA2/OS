#include <styleos/runtime_irq_859.h>
#include <assert.h>
#include <stdio.h>
static _Alignas(4096) uint64_t tables[PG_TABLE_PAGES][512];
int main(void){
 struct pg_plan p={.arch=PG_X86,.pa_bits=36,.tables=tables,.tables_pa=0x100000,.capacity=PG_TABLE_PAGES,.maps={{0xfee00000,0xfee00000,4096,PG_WRITE},{0xfec00000,0xfec00000,4096,PG_WRITE}},.count=2,.probe=PG_PROBE};
 struct s842_cache c={.pat=0x0007040600070406,.cap=0,.def=0x800,.bits=36};
 struct pg_result r=pg_build(&p);assert(r.error==PG_OK);unsigned n=0,yes=0;
 for(unsigned d=0;d<2;d++){
  uint64_t va=p.maps[d].va;size_t t=0;uint64_t *path[4];
  for(unsigned l=4;l>1;l--){path[4-l]=&tables[t][(va>>(12+9*(l-1)))&511u];t=(*path[4-l]&0x0000000ffffff000ULL)-p.tables_pa;t/=4096;}
  path[3]=&tables[t][(va>>12)&511u];*path[3]|=0x18;
  struct pg_lookup lookup;assert(pg_lookup(&p,va,&lookup)==PG_RIGHTS);assert(irq859_uc_page(&p,&r,&c,va));yes++;
  for(unsigned level=0;level<4;level++)for(unsigned bit=0;bit<64;bit++){
   uint64_t save=*path[level];*path[level]^=UINT64_C(1)<<bit;
   /* Changes to A/D and valid pointers to another page are not all invalid;
      only the exact path to the expected identity leaf must pass. */
   bool got=irq859_uc_page(&p,&r,&c,va);if(bit==5||(level==3&&bit==6)){assert(got);yes++;}else{assert(!got);n++;}*path[level]=save;
  }
  struct s842_cache bad=c;bad.pat=0x0606060606060606ULL;assert(!irq859_uc_page(&p,&r,&bad,va));n++;
  bad=c;bad.def=0x806;assert(!irq859_uc_page(&p,&r,&bad,va));n++;
  bad=c;bad.def=0;assert(!irq859_uc_page(&p,&r,&bad,va));n++;
  struct pg_result wrong=r;wrong.root+=4096;assert(!irq859_uc_page(&p,&wrong,&c,va));n++;
  wrong=r;wrong.used=1;assert(!irq859_uc_page(&p,&wrong,&c,va));n++;
 }
 assert(!irq859_uc_page(&p,&r,&c,0xfed00000));n++;
 printf("UC APIC leaf: %u positive / %u refusals; original pg_lookup refuses exact UC leaves\n",yes,n);
}
