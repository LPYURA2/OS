#include <styleos/tss_descriptor_783.h>
#define LOW_LIMIT (UINT64_C(1)<<47)
static uint64_t get64(const uint8_t *p) {
    uint64_t v=0;for(unsigned i=0;i<8u;++i)v|=(uint64_t)p[i]<<(8u*i);return v;
}
static void put64(uint8_t *p,uint64_t v) {
    for(unsigned i=0;i<8u;++i)p[i]=(uint8_t)(v>>(8u*i));
}
static int no_overlap(const void *a,size_t alen,const void *b,size_t blen) {
    uintptr_t aa=(uintptr_t)a,bb=(uintptr_t)b;
    return aa<=UINTPTR_MAX-alen && bb<=UINTPTR_MAX-blen &&
           (aa+alen<=bb || bb+blen<=aa);
}
static int tss_ok(const uint8_t *tss,size_t size,const struct tis782_plan *stage) {
    if(!tss || size!=TIS782_TSS_SIZE || !stage || !stage->staged ||
       stage->ltr_permitted || !stage->rsp0_top || !stage->ist1_top ||
       (stage->rsp0_top&15u) || (stage->ist1_top&15u) ||
       stage->rsp0_top>=LOW_LIMIT || stage->ist1_top>=LOW_LIMIT ||
       get64(tss+4u)!=stage->rsp0_top || get64(tss+36u)!=stage->ist1_top ||
       tss[102u]!=TIS782_TSS_SIZE || tss[103u]!=0) return 0;
    for(unsigned i=0;i<size;++i) {
        if((i>=4u && i<12u)||(i>=36u && i<44u)||i==102u)continue;
        if(tss[i])return 0;
    }return 1;
}
static int code_ok(uint64_t d) {
    uint8_t a=(uint8_t)(d>>40u);
    return (a&0xfeu)==0x9au && (d&(UINT64_C(1)<<53u)) &&
           !(d&(UINT64_C(1)<<54u));
}
enum tgd783_status tgd783_build(const uint8_t *old_gdt,size_t old_bytes,
    uint16_t cs,uint16_t tr,uint64_t tss_base,
    const uint8_t *tss_bytes,size_t tss_size,const struct tis782_plan *stage,
    uint8_t *new_gdt,size_t new_bytes,struct tgd783_result *out)
{
    if(out)*out=(struct tgd783_result){0};
    if(!out || !old_gdt || !new_gdt || !tss_bytes || !stage ||
       new_bytes!=TGD783_GDT_BYTES || !old_bytes ||
       old_bytes>TGD783_GDT_BYTES-16u || (old_bytes&7u) ||
       !cs || (cs&7u) || tr || !tss_base || (tss_base&4095u) ||
       tss_base>=LOW_LIMIT-4096u) return TGD783_ARGUMENT;
    if((size_t)cs+8u>old_bytes)return TGD783_GDT_BOUNDS;
    if(!no_overlap(old_gdt,old_bytes,new_gdt,new_bytes) ||
       !no_overlap(tss_bytes,tss_size,new_gdt,new_bytes)) return TGD783_ALIAS;
    if(!code_ok(get64(old_gdt+cs)))return TGD783_CODE_DESCRIPTOR;
    if(!tss_ok(tss_bytes,tss_size,stage))return TGD783_TSS_INVALID;
    /* Validate every input before first destination write. Preserve legacy
     * selectors, append one AVAILABLE long-mode TSS descriptor (type=0x9). */
    for(size_t i=0;i<new_bytes;++i)new_gdt[i]=0;
    for(size_t i=0;i<old_bytes;++i)new_gdt[i]=old_gdt[i];
    uint8_t *d=new_gdt+old_bytes;
    uint64_t limit=TIS782_TSS_SIZE-1u;
    uint64_t low=(limit&0xffffu)|((tss_base&0xffffffu)<<16u)|
        (UINT64_C(0x89)<<40u)|(((limit>>16u)&15u)<<48u)|
        (((tss_base>>24u)&255u)<<56u);
    put64(d,low);put64(d+8u,tss_base>>32u);
    *out=(struct tgd783_result){(uint16_t)old_bytes,
        (uint16_t)(old_bytes+15u),tss_base,stage->rsp0_top,
        stage->ist1_top,true,false,false};
    return TGD783_OK;
}
bool tgd783_verify(const uint8_t *gdt,size_t len,const uint8_t *tss,
    size_t tss_size,const struct tgd783_result *r)
{
    if(!gdt || len!=TGD783_GDT_BYTES || !r || !r->descriptor_valid ||
       r->active || r->guard_verified || !r->tss_selector ||
       (r->tss_selector&7u) ||
       (unsigned)r->new_gdt_limit!=(unsigned)r->tss_selector+15u ||
       r->new_gdt_limit>=TGD783_GDT_BYTES || !r->tss_base ||
       (r->tss_base&4095u) || r->tss_base>=LOW_LIMIT-4096u)return false;
    struct tis782_plan p={r->rsp0_top,r->ist1_top,true,false};
    if(!tss_ok(tss,tss_size,&p))return false;
    const uint8_t *d=gdt+r->tss_selector;
    uint64_t low=get64(d),high=get64(d+8u);
    uint64_t limit=(low&0xffffu)|(((low>>48u)&15u)<<16u);
    uint64_t addr=((low>>16u)&0xffffffu)|((low>>56u)<<24u)|(high<<32u);
    return limit==TIS782_TSS_SIZE-1u && addr==r->tss_base &&
           ((low>>40u)&255u)==0x89u && ((low>>52u)&15u)==0u &&
           (high>>16u)==0u;
}
