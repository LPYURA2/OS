#include <styleos/cache_policy_800.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
 struct cache800_pair p[2]={{UINT64_C(0x80000006), UINT64_C(0xfffff800)},
                             {UINT64_C(0x80000000), UINT64_C(0xfffff800)}};
 struct cache800_result r;
 /* Two overlapping types and UC dominance. */
 assert(cache800_physical_type(0x80000000,40,2,0x806,p,2,&r));
 assert(r.type==0 && r.matched==2 && r.mtrr_enabled);
 assert(cache800_physical_type(0x90000000,40,2,0x806,p,2,&r));
 assert(r.type==6 && r.matched==0);
 assert(cache800_physical_type(0x80000000,40,2,6,p,2,&r));
 assert(r.type==CACHE800_UNRESOLVED && !r.mtrr_enabled);
 assert(!cache800_physical_type(UINT64_C(1)<<40,40,2,0x806,p,2,&r));
 assert(!cache800_physical_type(0x80000000,40,3,0x806,p,2,&r));
 assert(!cache800_physical_type(0x80000000,35,2,0x806,p,2,&r));
 puts("STYLEOS 800 host MTRR evaluator PASS");return 0;
}
