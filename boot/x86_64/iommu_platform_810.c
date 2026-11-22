#include "efi.h"
#include <styleos/iommu_platform_810.h>
#include <styleos/dmar_808.h>
#include <styleos/acpi_madt_806.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
struct cfg810{struct efi_guid guid;void *table;};
static enum iommu810_backend selected;
static struct iommu810_ivrs ivrs;
static struct dmar808_info intel;
static int acpi_valid;
static unsigned root_count;
static int equal(const struct efi_guid *a,const struct efi_guid *b){
 if(a->a!=b->a||a->b!=b->b||a->c!=b->c)return 0;
 for(unsigned i=0;i<8;i++)if(a->d[i]!=b->d[i])return 0;
 return 1;
}
static uint32_t r32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void h(uint64_t x){arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(x>>(unsigned)i)&15]);}
static void f(const char *s,uint64_t v){arch_putc(' ');arch_puts(s);arch_putc('=');h(v);}
void iommu810_capture(const struct efi_system_table *st){
 static const struct efi_guid guid={0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
 int dm=0,iv=0,dv=0,ivv=0;
 acpi_valid=0;selected=IOMMU810_REFUSED;root_count=0;
 if(!st||!st->configuration_table||!st->number_of_table_entries||st->number_of_table_entries>256)return;
 const struct cfg810 *c=st->configuration_table;const uint8_t *rsdp=0;
 for(size_t j=0;j<st->number_of_table_entries;j++)if(equal(&c[j].guid,&guid)){
  if(rsdp)return;rsdp=(const uint8_t *)c[j].table;
 }
 uint64_t addr=0;
 if(!rsdp||!acpi806_rsdp(rsdp,36,&addr))return;
 const uint8_t *xsdt=(const uint8_t *)(uintptr_t)addr;
 uint32_t length=r32(xsdt+4);
 if(length<44||length>36+ACPI806_MAX_ROOT*8u)return;
 uint64_t entries[ACPI806_MAX_ROOT]={0};size_t n=0;
 if(!acpi806_xsdt(xsdt,length,entries,&n))return;
 root_count=(unsigned)n;
 for(size_t i=0;i<n;i++){
  const uint8_t *p=(const uint8_t *)(uintptr_t)entries[i];
  if(p[0]=='D'&&p[1]=='M'&&p[2]=='A'&&p[3]=='R'){
   if(dm++)return;
   uint32_t size=r32(p+4);
   if(size>=48&&size<=DMAR808_MAX_TABLE)dv=dmar808_parse(p,size,&intel);
  }else if(p[0]=='I'&&p[1]=='V'&&p[2]=='R'&&p[3]=='S'){
   if(iv++)return;
   uint32_t size=r32(p+4);
   if(size>=48&&size<=IOMMU810_MAX_TABLE)ivv=ivrs810_parse(p,size,&ivrs);
  }
 }
 selected=iommu810_select(dm,dv,iv,ivv);
 acpi_valid=1;
}
static _Noreturn void die(const char *m){arch_puts("STYLEOS S810 FAIL ");arch_puts(m);arch_putc('\n');arch_halt();}
_Noreturn void iommu810_terminal(const struct boot_info *b){
 if(!b||b->magic!=STYLEOS_BOOT_MAGIC||b->flags!=STYLEOS_BOOT_EXITED_SERVICES)die("handoff");
 if(!acpi_valid||selected==IOMMU810_REFUSED)die("acpi-or-conflicting-vendors");
 arch_puts("STYLEOS S810 PLATFORM");f("backend",selected);f("root-entries",root_count);arch_putc('\n');
 if(selected==IOMMU810_INTEL){
  arch_puts("STYLEOS S810 INTEL");f("drhd",intel.drhd_count);f("rmrr",intel.rmrr_count);f("unknown",intel.unknown_count);arch_putc('\n');
  for(unsigned i=0;i<intel.drhd_count;i++){
   arch_puts("STYLEOS S810 UNIT");f("type",0);f("index",i);f("segment",intel.drhd[i].segment);
   f("base",intel.drhd[i].base);f("scope-count",intel.drhd[i].scope_count);arch_putc('\n');
  }
 }else if(selected==IOMMU810_AMD){
  arch_puts("STYLEOS S810 AMD");f("ivhd",ivrs.ivhd_count);f("ivmd",ivrs.ivmd_count);f("unknown",ivrs.unknown_blocks);f("opaque-scopes",ivrs.opaque_entries);arch_putc('\n');
  for(unsigned i=0;i<ivrs.ivhd_count;i++){
   arch_puts("STYLEOS S810 UNIT");f("type",ivrs.units[i].type);f("index",i);f("segment",ivrs.units[i].pci_segment);
   f("base",ivrs.units[i].mmio_base);f("scope-count",0);arch_putc('\n');
  }
 }
 arch_puts("STYLEOS S810 POLICY inventory-only=yes pci-requesters-verified=no dma-domains-verified=no dma-access-authorized=no dma-isolation-verified=no production-cr3=deny\n");
 arch_puts("STYLEOS S810 PASS\n");
 __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));arch_halt();
}
