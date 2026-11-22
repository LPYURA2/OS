#include <styleos/tss_descriptor_783.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t old[72],gdt[4096],tss[104];
static struct tgd783_result result;
static struct tis782_plan stage={0x172ff0,0x173ff0,true,false};
static void put64(unsigned off,uint64_t v) {
    for(unsigned i=0;i<8u;++i)old[off+i]=(uint8_t)(v>>(i*8u));
}
static void reset(void) {
    memset(old,0,sizeof old);memset(gdt,0xa5,sizeof gdt);
    memset(tss,0,sizeof tss);
    put64(0x38,UINT64_C(0x00af9a000000ffff));
    assert(tis782_encode(&stage,tss,sizeof tss));
}
static enum tgd783_status build(void) {
    return tgd783_build(old,sizeof old,0x38,0,0x171000,
        tss,sizeof tss,&stage,gdt,sizeof gdt,&result);
}
static void denied(void) {
    for(size_t i=0;i<sizeof gdt;++i)assert(gdt[i]==0xa5);
    assert(!result.descriptor_valid && !result.active);
}
int main(void) {
    reset();assert(build()==TGD783_OK);
    assert(result.tss_selector==0x48 && result.new_gdt_limit==0x57 &&
           !result.active && !result.guard_verified);
    assert(memcmp(old,gdt,sizeof old)==0);
    assert(tgd783_verify(gdt,sizeof gdt,tss,sizeof tss,&result));
    assert(gdt[0x48+5]==0x89 && gdt[0x48]==103);
    gdt[0x48+5]=0x8b;
    assert(!tgd783_verify(gdt,sizeof gdt,tss,sizeof tss,&result));
    gdt[0x48+5]=0x89;
    tss[36]^=1;assert(!tgd783_verify(gdt,sizeof gdt,tss,sizeof tss,&result));
    tss[36]^=1;
    result.active=true;
    assert(!tgd783_verify(gdt,sizeof gdt,tss,sizeof tss,&result));
    result.active=false;
    assert(tgd783_verify(gdt,sizeof gdt,tss,sizeof tss,&result));
    reset();old[0x38+5]=0x92;assert(build()==TGD783_CODE_DESCRIPTOR);denied();
    reset();old[0x38+6]|=0x40;assert(build()==TGD783_CODE_DESCRIPTOR);denied();
    reset();tss[102]=0;assert(build()==TGD783_TSS_INVALID);denied();
    reset();tss[20]=0x42;assert(build()==TGD783_TSS_INVALID);denied();
    reset();stage.ltr_permitted=true;
    assert(build()==TGD783_TSS_INVALID);denied();stage.ltr_permitted=false;
    reset();assert(tgd783_build(old,72,0x39,0,0x171000,
        tss,104,&stage,gdt,4096,&result)==TGD783_ARGUMENT);denied();
    reset();assert(tgd783_build(old,72,0x38,0x40,0x171000,
        tss,104,&stage,gdt,4096,&result)==TGD783_ARGUMENT);denied();
    reset();assert(tgd783_build(old,72,0x38,0,0x171001,
        tss,104,&stage,gdt,4096,&result)==TGD783_ARGUMENT);denied();
    reset();assert(tgd783_build(old,64,0x40,0,0x171000,
        tss,104,&stage,gdt,4096,&result)==TGD783_GDT_BOUNDS);denied();
    reset();assert(tgd783_build(old,72,0x38,0,0x171000,
        tss,104,&stage,old,4096,&result)==TGD783_ALIAS); 
    reset();assert(build()==TGD783_OK);
    puts("STYLEOS 783 GDT TSS DESCRIPTOR HOST PASS staged=1 ltr=0 guard=unproven");
    return 0;
}
