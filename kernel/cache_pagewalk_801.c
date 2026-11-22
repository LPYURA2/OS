#include <styleos/cache_pagewalk_801.h>
#include <stddef.h>
static bool canonical(uint64_t v){
    uint64_t hi=v>>48;
    return hi==(((v>>47)&1u)?UINT64_C(0xffff):0u);
}
/* No reads from the target MMIO VA. All reading is through callback-supplied
 * already-mapped table pages. Caller must validate these pages separately. */
bool cache801_walk(uint64_t cr3,uint64_t va,uint64_t pat,uint8_t physbits,
                   cache801_read_entry read_entry,void *ctx,
                   struct cache801_translation *out){
    if(!out || !read_entry || physbits<36 || physbits>52 || !canonical(va))return false;
    *out=(struct cache801_translation){0};
    const uint64_t physmask=((UINT64_C(1)<<physbits)-1u)&~UINT64_C(0xfff);
    uint64_t table=cr3&~UINT64_C(0xfff);
    if(!table || (table&~physmask))return false;
    for(unsigned level=4;level>=1;--level){
        uint16_t slot=(uint16_t)((va>>(12u+9u*(level-1u)))&511u);
        uint64_t e=0;
        if(!read_entry(ctx,table,slot,&e))return false;
        if(!(e&1u))return true;
        /* Large pages are allowed at PDPT (1GiB) or PD (2MiB).
         * PS bit in PML4 is invalid; PTE bit7 means PAT, not PS. */
        if(level==4 && (e&(UINT64_C(1)<<7)))return false;
        bool leaf=(level==1)||(level==3 && (e&(UINT64_C(1)<<7)))||
                   (level==2 && (e&(UINT64_C(1)<<7)));
        if(leaf){
            unsigned shift=12u+9u*(level-1u);
            uint64_t pagesize=UINT64_C(1)<<shift;
            uint64_t base=e&physmask&~(pagesize-1u);
            uint64_t forbidden=((pagesize-1u)&~UINT64_C(0xfff));
            if(level>1){
                /* On large leaves bit12 denotes PAT; all other lower
                 * reserved physical bits must be clear. */
                forbidden &=~(UINT64_C(1)<<12);
                if(e&forbidden)return false;
            }
            unsigned patbit=level==1?7u:12u;
            uint8_t idx=(uint8_t)((((e>>patbit)&1u)<<2)|(((e>>4)&1u)<<1)|((e>>3)&1u));
            *out=(struct cache801_translation){true,e,base+(va&(pagesize-1u)),
                                               pagesize,idx,(uint8_t)((pat>>(8u*idx))&255u)};
            return true;
        }
        table=e&physmask;
        if(!table || (e&(UINT64_C(1)<<7)))return false;
    }
    return false;
}
