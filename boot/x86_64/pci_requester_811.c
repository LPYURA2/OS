#include "efi.h"
#include <styleos/pci_requester_811.h>
#include <styleos/dmar_808.h>
#include <styleos/iommu_platform_810.h>
#include <styleos/acpi_madt_806.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
/* S811 is terminal diagnostic only. ACPI is copied into image-owned storage
 * before EBS; no UEFI services or firmware table pointers are used afterwards. */
struct cfg811 {struct efi_guid guid;void *table;};
static struct pci811_map map811;
static enum iommu810_backend backend811=IOMMU810_REFUSED;
static unsigned root811;
static int valid811;
static int eq(const struct efi_guid *a,const struct efi_guid *b){
 if(a->a!=b->a||a->b!=b->b||a->c!=b->c)return 0;
 for(unsigned i=0;i<8;i++)if(a->d[i]!=b->d[i])return 0;
 return 1;
}
static uint32_t r32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void h(uint64_t n){arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(n>>(unsigned)i)&15]);}
static void f(const char *key,uint64_t n){arch_putc(' ');arch_puts(key);arch_putc('=');h(n);}
void pci811_capture(const struct efi_system_table *st){
 static const struct efi_guid guid={0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
 valid811=0;backend811=IOMMU810_REFUSED;root811=0;
 if(!st||!st->configuration_table||!st->number_of_table_entries||st->number_of_table_entries>256)return;
 const struct cfg811 *c=st->configuration_table;const uint8_t *rsdp=0;
 for(size_t i=0;i<st->number_of_table_entries;i++)if(eq(&c[i].guid,&guid)){
  if(rsdp)return;rsdp=(const uint8_t *)c[i].table;
 }
 uint64_t addr=0;
 if(!rsdp||!acpi806_rsdp(rsdp,36,&addr))return;
 const uint8_t *xsdt=(const uint8_t *)(uintptr_t)addr;
 uint32_t length=r32(xsdt+4);
 if(length<44||length>36+ACPI806_MAX_ROOT*8u)return;
 uint64_t entries[ACPI806_MAX_ROOT]={0};size_t n=0;
 if(!acpi806_xsdt(xsdt,length,entries,&n))return;
 root811=(unsigned)n;
 int dm=0,iv=0,dv=0,ivv=0;struct pci811_map i_map={0},a_map={0};
 for(size_t i=0;i<n;i++){
  const uint8_t *p=(const uint8_t *)(uintptr_t)entries[i];
  if(p[0]=='D'&&p[1]=='M'&&p[2]=='A'&&p[3]=='R'){
   if(dm++)return;
   unsigned sz=r32(p+4);
   if(sz>=48&&sz<=DMAR808_MAX_TABLE)dv=pci811_dmar_decode(p,sz,&i_map);
  }else if(p[0]=='I'&&p[1]=='V'&&p[2]=='R'&&p[3]=='S'){
   if(iv++)return;
   unsigned sz=r32(p+4);
   if(sz>=48&&sz<=IOMMU810_MAX_TABLE)ivv=pci811_ivrs_decode(p,sz,&a_map);
  }
 }
 backend811=iommu810_select(dm,dv,iv,ivv);
 if(backend811==IOMMU810_REFUSED)return;
 if(backend811==IOMMU810_INTEL)map811=i_map;
 else if(backend811==IOMMU810_AMD)map811=a_map;
 else map811=(struct pci811_map){0};
 valid811=1;
}
/* CF8 is only the legacy PCI config SELECT register; data is read-only CFC.
 * Limit enumeration to bus 0; downstream bridge paths remain unproven. */
static uint32_t pci_read(uint8_t bus,uint8_t dev,uint8_t fun){
 uint32_t addr=0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fun<<8);
 __asm__ volatile("outl %0,%%dx"::"a"(addr),"d"((uint16_t)0xcf8u):"memory");
 uint32_t value;__asm__ volatile("inl %%dx,%0":"=a"(value):"d"((uint16_t)0xcfcu):"memory");return value;
}
static _Noreturn void die(const char *why){arch_puts("STYLEOS S811 FAIL ");arch_puts(why);arch_putc('\n');arch_halt();}
_Noreturn void pci811_terminal(const struct boot_info *boot){
 if(!boot||boot->magic!=STYLEOS_BOOT_MAGIC||boot->flags!=STYLEOS_BOOT_EXITED_SERVICES)die("handoff");
 if(!valid811||backend811==IOMMU810_REFUSED)die("acpi");
 unsigned present=0,matched=0,unknown=0;uint8_t owner=0;
 for(unsigned d=0;d<32;d++)for(unsigned fn=0;fn<8;fn++){
  uint32_t id=pci_read(0,(uint8_t)d,(uint8_t)fn);
  if((id&0xffffu)==0xffffu||!id)continue;
  present++;
  uint16_t rid=(uint16_t)((d<<3)|fn);
  int match=pci811_lookup(&map811,rid,&owner);
  if(match==1)matched++;
  else unknown++;
 }
 arch_puts("STYLEOS S811 PLATFORM");f("backend",(uint64_t)backend811);f("root-entries",root811);arch_putc('\n');
 arch_puts("STYLEOS S811 SCOPE");f("units",map811.units);f("exact",map811.exact_count);
 f("unresolved",map811.unresolved);f("include-all",map811.include_all);f("ranges",map811.ranges);
 f("collisions",map811.collisions);arch_putc('\n');
 arch_puts("STYLEOS S811 PCI");f("bus",0);f("present",present);f("explicit-matches",matched);f("unresolved",unknown);arch_putc('\n');
 if(present==0||matched>present||matched+unknown!=present||map811.collisions)die("pci-scope");
 arch_puts("STYLEOS S811 POLICY pci-bus0-only=yes bridge-hierarchy-verified=no device-ownership-complete=no dma-domains-verified=no dma-access-authorized=no dma-isolation-verified=no production-cr3=deny\n");
 arch_puts("STYLEOS S811 PASS\n");
 __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));arch_halt();
}
