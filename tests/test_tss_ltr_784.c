#include <styleos/tss_ltr_trial_784.h>
#include <styleos/tss_ist_stage_782.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t old[72],gdt[4096],tss[104],idt[4096];
static struct tgd783_result descriptor;
static const struct tis782_plan staged={0x172ff0,0x173ff0,true,false};
static const struct tlt784_context allowed={0x40000,0x105000,0x107000,
    0x47,0xfff,0x38,0,true,true,true};

static void write64(uint8_t *p,uint64_t value) {
    for(unsigned i=0;i<8u;++i)p[i]=(uint8_t)(value>>(8u*i));
}
static void init(void) {
    memset(old,0,sizeof old);
    memset(idt,0,sizeof idt);
    write64(old+0x38,UINT64_C(0x00af9a000000ffff));
    for(unsigned i=0;i<256u;++i) {
        uint8_t *gate=idt+16u*i;
        uint64_t ip=0x180000u+16u*i;
        gate[0]=(uint8_t)ip;gate[1]=(uint8_t)(ip>>8);
        gate[2]=0x38;gate[3]=0;
        gate[4]=0;gate[5]=0x8e;
        gate[6]=(uint8_t)(ip>>16);gate[7]=(uint8_t)(ip>>24);
    }
    assert(tis782_encode(&staged,tss,sizeof tss));
    assert(tgd783_build(old,sizeof old,0x38,0,0x171000,tss,sizeof tss,
                       &staged,gdt,sizeof gdt,&descriptor)==TGD783_OK);
}
static enum tlt784_status check(const struct tlt784_context *c) {
    return tlt784_preflight(c,&descriptor,gdt,sizeof gdt,tss,sizeof tss,
                            idt,sizeof idt);
}
int main(void) {
    init();
    uint8_t snapshot_gdt[4096],snapshot_tss[104];
    memcpy(snapshot_gdt,gdt,sizeof gdt);
    memcpy(snapshot_tss,tss,sizeof tss);
    assert(check(&allowed)==TLT784_OK);
    assert(memcmp(gdt,snapshot_gdt,sizeof gdt)==0);
    assert(memcmp(tss,snapshot_tss,sizeof tss)==0);
    struct tlt784_context changed=allowed;
    changed.tr=0x48;assert(check(&changed)==TLT784_BAD_CONTEXT);
    changed=allowed;changed.interrupts_disabled=false;
    assert(check(&changed)==TLT784_BAD_CONTEXT);
    changed=allowed;changed.idt_mapped=false;
    assert(check(&changed)==TLT784_BAD_CONTEXT);
    changed=allowed;changed.pages_attested=false;
    assert(check(&changed)==TLT784_BAD_CONTEXT);
    changed=allowed;changed.idt_limit=0xfee;
    assert(check(&changed)==TLT784_BAD_CONTEXT);
    changed=allowed;changed.cs=0x3b;
    assert(check(&changed)==TLT784_BAD_CONTEXT);
    changed=allowed;changed.old_cr3=0;
    assert(check(&changed)==TLT784_BAD_CONTEXT);
    changed=allowed;changed.old_limit=0x3f;
    assert(check(&changed)==TLT784_BAD_DESCRIPTOR);
    gdt[0x48+5]=0x8b;
    assert(check(&allowed)==TLT784_BAD_DESCRIPTOR);gdt[0x48+5]=0x89;
    descriptor.active=true;
    assert(check(&allowed)==TLT784_BAD_DESCRIPTOR);descriptor.active=false;
    descriptor.guard_verified=true;
    assert(check(&allowed)==TLT784_BAD_DESCRIPTOR);descriptor.guard_verified=false;
    idt[5]=0xee;
    assert(check(&allowed)==TLT784_BAD_HANDLERS);idt[5]=0x8e;
    idt[4]=1;
    assert(check(&allowed)==TLT784_BAD_HANDLERS);idt[4]=0;
    idt[2]=0x40;
    assert(check(&allowed)==TLT784_BAD_HANDLERS);idt[2]=0x38;
    assert(check(&allowed)==TLT784_OK);
    assert(memcmp(gdt,snapshot_gdt,sizeof gdt)==0);
    assert(memcmp(tss,snapshot_tss,sizeof tss)==0);
    puts("STYLEOS 784 TERMINAL TSS PREFLIGHT HOST PASS negatives=13 mutations=0 ist=0");
    return 0;
}
