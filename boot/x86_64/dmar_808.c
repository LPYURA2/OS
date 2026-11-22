#include "efi.h"
#include <styleos/dmar_808.h>
#include <styleos/acpi_madt_806.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
/* Diagnostic-only: read OVMF ACPI before EBS. Do not enable DMA remapping. */
struct cfg808 {struct efi_guid guid;void *table;};
static struct dmar808_info saved;
static int observed; /* -1 invalid ACPI/DMAR, 0 absent, 1 parsed */
static unsigned root_count;
static int eq(const struct efi_guid *a,const struct efi_guid *b){
 if(a->a!=b->a||a->b!=b->b||a->c!=b->c)return 0;
 for(unsigned i=0;i<8;i++)if(a->d[i]!=b->d[i])return 0;return 1;
}
static uint32_t l32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void hx(uint64_t value){arch_puts("0x");for(int s=60;s>=0;s-=4)arch_putc("0123456789abcdef"[(value>>(unsigned)s)&15]);}
static void field(const char *name,uint64_t value){arch_putc(' ');arch_puts(name);arch_putc('=');hx(value);}
void dmar808_capture(const struct efi_system_table *st){
 static const struct efi_guid acpi20={0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
 observed=-1;root_count=0;
 if(!st||!st->configuration_table||st->number_of_table_entries==0||st->number_of_table_entries>256)return;
 const struct cfg808 *cfg=st->configuration_table;const uint8_t *rsdp=0;
 for(size_t i=0;i<st->number_of_table_entries;i++)if(eq(&cfg[i].guid,&acpi20)){
  if(rsdp)return;rsdp=(const uint8_t *)cfg[i].table;
 }
 uint64_t xsdt_addr=0;
 if(!rsdp||!acpi806_rsdp(rsdp,36,&xsdt_addr))return;
 const uint8_t *xsdt=(const uint8_t *)(uintptr_t)xsdt_addr;
 uint32_t len=l32(xsdt+4);
 if(len<44||len>36+ACPI806_MAX_ROOT*8u)return;
 uint64_t entries[ACPI806_MAX_ROOT]={0};size_t n=0;
 if(!acpi806_xsdt(xsdt,len,entries,&n))return;
 root_count=(unsigned)n;observed=0;
 for(size_t i=0;i<n;i++){
  const uint8_t *p=(const uint8_t *)(uintptr_t)entries[i];
  if(p[0]!='D'||p[1]!='M'||p[2]!='A'||p[3]!='R')continue;
  if(observed==1){observed=-1;return;}
  uint32_t size=l32(p+4);
  if(size<48||size>DMAR808_MAX_TABLE||!dmar808_parse(p,size,&saved)){
    observed=-1;return;
  }
  observed=1;
 }
}
static _Noreturn void bad(const char *why){arch_puts("STYLEOS S808 FAIL ");arch_puts(why);arch_putc('\n');arch_halt();}
_Noreturn void dmar808_terminal(const struct boot_info *b){
 if(!b||b->magic!=STYLEOS_BOOT_MAGIC||b->flags!=STYLEOS_BOOT_EXITED_SERVICES)bad("handoff");
 if(observed<0)bad("acpi-dmar-invalid");
 arch_puts("STYLEOS S808 STATUS");field("present",observed==1);field("root-entries",root_count);arch_putc('\n');
 if(observed){
  arch_puts("STYLEOS S808 DMAR");field("length",saved.length);field("revision",saved.revision);
  field("host-aw",saved.host_address_width);field("flags",saved.flags);field("drhd-count",saved.drhd_count);
  field("rmrr-count",saved.rmrr_count);field("unknown-count",saved.unknown_count);arch_putc('\n');
  for(unsigned i=0;i<saved.drhd_count;i++){
   const struct dmar808_drhd *u=&saved.drhd[i];
   arch_puts("STYLEOS S808 DRHD");field("index",i);field("segment",u->segment);field("flags",u->flags);
   field("base",u->base);field("scope-count",u->scope_count);arch_putc('\n');
  }
  for(unsigned i=0;i<saved.rmrr_count;i++){
   const struct dmar808_rmrr *u=&saved.rmrr[i];
   arch_puts("STYLEOS S808 RMRR");field("index",i);field("segment",u->segment);
   field("base",u->base);field("limit",u->limit_inclusive);field("scope-count",u->scope_count);arch_putc('\n');
  }
 }
 arch_puts("STYLEOS S808 POLICY dmar-discovery-verified=yes iommu-registers-observed=no iommu-translation-enabled=no dma-isolation-verified=no dma-access-authorized=no uefi-runtime-verified=no production-cr3=deny\n");
 arch_puts("STYLEOS S808 PASS\n");
 __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));arch_halt();
}

/* Validated ACPI snapshot, only for the separate read-only S809 VT-d probe. */
int dmar808_get_info(struct dmar808_info *out){
 if(observed==1 && out)*out=saved;
 return observed==1 && !out ? -1 : observed;
}
