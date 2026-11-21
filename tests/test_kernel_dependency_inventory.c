#include <styleos/kernel_dependency_inventory.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* A small loaded-format PE32+, not raw on-disk file offsets. */
static unsigned char pe[4*4096];
static void put16(size_t off,unsigned x)
{ pe[off]=(unsigned char)x; pe[off+1]=(unsigned char)(x>>8); }
static void put32(size_t off,uint32_t x)
{ for(unsigned i=0;i<4;++i)pe[off+i]=(unsigned char)(x>>(i*8)); }
static void section(unsigned i,uint32_t va,uint32_t size,uint32_t flags)
{ size_t off=0x80+24+240+40u*i;put32(off+8,size);put32(off+12,va);put32(off+36,flags); }
int main(void)
{
    pe[0]='M';pe[1]='Z';put32(0x3c,0x80);pe[0x80]='P';pe[0x81]='E';
    put16(0x80+6,3);put16(0x80+20,240);put16(0x80+24,0x20b);
    put32(0x80+24+56,sizeof pe);put32(0x80+24+60,0x400);
    section(0,0x1000,0x1000,0x60000020u);
    section(1,0x2000,0x1000,0xc0000040u);
    section(2,0x3000,0x1000,0x40000040u);
    struct kdi_pe_profile profile;
    assert(kdi_profile_loaded_pe(pe,sizeof pe,&profile));
    assert(profile.image_pages==4 && profile.text_pages==1 &&
           profile.writable_pages==1 && profile.readonly_pages==2 &&
           !profile.mixed_pages && !profile.uncovered_pages);
    struct kdi_live_dependencies d={.inherited_rwx_pages=3};
    struct kdi_report r={0};
    assert(kdi_audit_dependencies(&profile,&d,&r));
    assert(r.blockers==(KDI_TSS_IST|KDI_OTHER_STACKS|KDI_FRAMEBUFFER|
        KDI_DEVICE_MMIO|KDI_FIRMWARE_RUNTIME|KDI_WX_POLICY|
        KDI_CACHE_POLICY|KDI_PHYSICAL_OWNERSHIP));
    assert(!r.activation_permitted);
    d.tr_selector=0x28;d.tss_ist_proved=true;d.all_stacks_proved=true;
    d.framebuffer_proved=true;d.device_mmio_proved=true;
    d.firmware_runtime_proved=true;d.kernel_wx_proved=true;
    d.cache_policy_proved=true;d.physical_ownership_proved=true;
    d.inherited_rwx_pages=0;
    assert(kdi_audit_dependencies(&profile,&d,&r) && r.blockers==0 &&
           !r.activation_permitted);
    section(2,0x2000,0x1000,0xe0000020u);
    assert(kdi_profile_loaded_pe(pe,sizeof pe,&profile));
    assert(profile.mixed_pages==1);
    assert(kdi_audit_dependencies(&profile,&d,&r) &&
           (r.blockers&KDI_PE_CONFLICT) && !r.activation_permitted);
    section(2,0x3000,0x1000,0x40000040u);
    assert(kdi_profile_loaded_pe(pe,sizeof pe,&profile));
    put16(0x80+6,33);assert(!kdi_profile_loaded_pe(pe,sizeof pe,&profile));
    put16(0x80+6,3);put32(0x80+24+56,sizeof pe+4096);
    assert(kdi_profile_loaded_pe(pe,sizeof pe,&profile));
    assert(profile.declared_extra_pages==1);
    assert(kdi_audit_dependencies(&profile,&d,&r) &&
           (r.blockers&KDI_PE_CONFLICT) && !r.activation_permitted);
    assert(!kdi_audit_dependencies(NULL,&d,&r) && !r.activation_permitted);
    assert(!kdi_audit_dependencies(&profile,&d,NULL));
    puts("STYLEOS 780 DEPENDENCY HOST PASS pe=1 blockers=1 fail-closed=1");
    return 0;
}
