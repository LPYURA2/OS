#include <styleos/root_minimization_789.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
static uint64_t old[512] __attribute__((aligned(4096)));
static uint64_t staged[512] __attribute__((aligned(4096)));
int main(void){
    const uint64_t base=UINT64_C(0x00100003), alias=UINT64_C(0x00800003);
    old[0]=base;old[23]=UINT64_C(0x00900003);old[510]=UINT64_C(0x01000003);
    memcpy(staged,old,sizeof staged);staged[R789_ALIAS_SLOT]=alias;
    const uint64_t need[]={UINT64_C(0x10000),UINT64_C(0x5f00000), UINT64_C(0x7fff1000)};
    assert(r789_prune_inherited_root(staged,old,need,3));
    assert(r789_audit_restricted_root(staged,old));
    staged[0]|=0x20u; /* CPU Accessed bit is an allowed hardware update. */
    assert(r789_audit_restricted_root(staged,old));
    staged[0]&=~UINT64_C(0x20);
    assert(staged[0]==base && staged[R789_ALIAS_SLOT]==alias);
    for(unsigned i=1;i<512;i++)if(i!=128)assert(staged[i]==0);
    staged[23]=old[23];assert(!r789_audit_restricted_root(staged,old));staged[23]=0;
    const uint64_t bad_need[]={UINT64_C(1)<<39};
    memcpy(staged,old,sizeof staged);staged[128]=alias;
    assert(!r789_prune_inherited_root(staged,old,bad_need,1));
    assert(staged[23]==old[23]);
    assert(!r789_prune_inherited_root(staged,old,need,0));
    assert(!r789_prune_inherited_root(staged,old,need,33));
    assert(!r789_prune_inherited_root(staged,staged,need,3));
    staged[91]=0x999;
    assert(!r789_prune_inherited_root(staged,old,need,3));
    assert(staged[91]==0x999);
    staged[91]=0;old[128]=0x200003;
    assert(!r789_prune_inherited_root(staged,old,need,3));
    old[128]=0;old[0]=0;
    assert(!r789_prune_inherited_root(staged,old,need,3));
    puts("STYLEOS 789 HOST PML4 MINIMIZATION PASS");return 0;
}
