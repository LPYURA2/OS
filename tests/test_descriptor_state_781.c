#include <styleos/descriptor_state_781.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t idt[4096];
static void make_idt(unsigned ist)
{
    memset(idt,0,sizeof(idt));
    for (unsigned n=0;n<256u;++n) {
        uint8_t *p=idt+16u*n;
        uint64_t target=UINT64_C(0x12345678)+n*32u;
        p[0]=(uint8_t)target;p[1]=(uint8_t)(target>>8u);
        p[2]=0x38u; p[3]=0;
        p[4]=(uint8_t)(n==17u ? ist : 0u);
        p[5]=0x8eu;
        p[6]=(uint8_t)(target>>16u);p[7]=(uint8_t)(target>>24u);
    }
}
int main(void)
{
    struct dsi781_report r={0};
    make_idt(0);
    assert(dsi781_audit_idt(idt, sizeof idt, 0x38, 0, 0x47, &r));
    assert(r.gates==256 && r.ist_gates==0 && r.ist_index_mask==0);
    assert(r.tss_descriptor_unresolved && r.tss_stack_unresolved &&
           !r.activation_permitted);
    assert(!dsi781_audit_idt(NULL,sizeof idt,0x38,0,0x47,&r));
    assert(!dsi781_audit_idt(idt,sizeof idt-1u,0x38,0,0x47,&r));
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,0,0x38,&r));
    assert(!dsi781_audit_idt(idt,sizeof idt,0x3b,0,0x47,&r));
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,0x38,0x47,&r));
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,0x48,0x47,&r));
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,1,0x47,&r));
    assert(dsi781_audit_idt(idt,sizeof idt,0x38,0x10,0x47,&r));
    assert(r.tr==0x10 && r.tss_descriptor_unresolved && !r.activation_permitted);
    make_idt(4);
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,0,0x47,&r));
    assert(dsi781_audit_idt(idt,sizeof idt,0x38,0x10,0x47,&r));
    assert(r.ist_gates==1 && r.ist_index_mask==8u);
    idt[17u*16u+4u]=0x84u;
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,0x10,0x47,&r));
    make_idt(0); idt[5]=0x0eu;
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,0,0x47,&r));
    make_idt(0); idt[2]=0x28u;
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,0,0x47,&r));
    make_idt(0); idt[12]=0x80u;
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,0,0x47,&r));
    make_idt(0); memset(idt,0,8u);
    assert(!dsi781_audit_idt(idt,sizeof idt,0x38,0,0x47,&r));
    assert(dsi781_current_rsp_in_boot_stack(0x1ff0u,0x1000u,0x2000u));
    assert(!dsi781_current_rsp_in_boot_stack(0x2000u,0x1000u,0x2000u));
    assert(!dsi781_current_rsp_in_boot_stack(0x1000u,0x2000u,0x1000u));
    assert(!dsi781_current_rsp_in_boot_stack(0x1000u,0,0x2000u));
    assert(!dsi781_current_rsp_in_boot_stack(0x1000u,0x1000u,UINT64_MAX));
    puts("STYLEOS 781 IDT TSS IST BOUNDS HOST PASS");
    return 0;
}
