#include <styleos/cache_policy_800.h>
#include <stddef.h>
static bool known(uint8_t t){return t==0u || t==1u || t==4u || t==5u || t==6u;}
bool cache800_physical_type(uint64_t addr,uint8_t bits,uint64_t cap,uint64_t def,
                           const struct cache800_pair *pairs,uint8_t count,
                           struct cache800_result *out)
{
    if (!out || bits<36u || bits>52u || count>CACHE800_MAX_VAR ||
        (count && !pairs) || count!=(uint8_t)(cap&255u))return false;
    uint64_t physical_limit=(UINT64_C(1)<<bits);
    if(addr>=physical_limit || (def&~UINT64_C(0xCFF)))return false;
    *out=(struct cache800_result){CACHE800_UNRESOLVED,0,false};
    /* If MTRRs disabled, no physical-type inference is authorized. */
    if(!(def&(1u<<11)))return true;
    out->mtrr_enabled=true;
    uint64_t high=((UINT64_C(1)<<bits)-1u)&~UINT64_C(0xfff);
    uint8_t winner=CACHE800_UNRESOLVED;
    bool conflicted=false;
    for(uint8_t i=0;i<count;i++){
        uint64_t base=pairs[i].base, mask=pairs[i].mask;
        if (!known((uint8_t)(base&255u)))return false;
        if(!(mask&(1u<<11)))continue;
        uint64_t m=mask&high;
        if((addr&m)!=(base&m))continue;
        if(out->matched==255)return false;
        out->matched++;
        uint8_t t=(uint8_t)(base&255u);
        if(conflicted)continue;
        if(winner==CACHE800_UNRESOLVED)winner=t;
        else if(winner==t){}
        else if(t==0u || winner==0u)winner=0u; /* UC dominant */
        else if((t==4u && winner==6u)||(t==6u && winner==4u))winner=4u;
        else {winner=CACHE800_UNRESOLVED;conflicted=true;} /* ambiguous conflict */
    }
    if(!out->matched)winner=(uint8_t)(def&255u);
    out->type=known(winner)?winner:CACHE800_UNRESOLVED;
    return true;
}
