#include "efi.h"
#include <styleos/acpi_madt_806.h>
#include <styleos/arch.h>
#include <styleos/boot_info.h>
#include <stdint.h>
#include <stddef.h>
/* Read only OVMF configuration tables before EBS; post-EBS prints image-owned
 * summary only. Never program IOAPIC, PIT, IDT or local APIC in this profile. */
static struct acpi806_topology captured;
static int ready;
static void hx(uint64_t v){arch_puts("0x");for(int k=60;k>=0;k-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)k)&15u]);}
static void fld(const char *name,uint64_t value){arch_putc(' ');arch_puts(name);arch_putc('=');hx(value);}
static int guid_equal(const struct efi_guid *a,const struct efi_guid *b){
 if(a->a!=b->a||a->b!=b->b||a->c!=b->c)return 0;
 for(unsigned i=0;i<8;i++)if(a->d[i]!=b->d[i])return 0;
 return 1;
}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int sig4(const uint8_t *p,const char *sig){for(unsigned i=0;i<4;i++)if(p[i]!=(uint8_t)sig[i])return 0;return 1;}
struct config_entry806 {struct efi_guid guid;void *address;};
void acpi806_capture(const struct efi_system_table *st){
 static const struct efi_guid v2={0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
 ready=0;
 if(!st||!st->configuration_table||st->number_of_table_entries==0||st->number_of_table_entries>256)return;
 const struct config_entry806 *cfg=st->configuration_table;
 const uint8_t *rsdp=NULL;
 for(size_t i=0;i<st->number_of_table_entries;i++)if(guid_equal(&cfg[i].guid,&v2)){
  if(rsdp)return;rsdp=cfg[i].address;
 }
 if(!rsdp)return;
 uint64_t xsdt_address=0; if(!acpi806_rsdp(rsdp,36,&xsdt_address))return;
 const uint8_t *xsdt=(const uint8_t *)(uintptr_t)xsdt_address;
 uint32_t length=le32(xsdt+4);
 if(length<44||length>36+ACPI806_MAX_ROOT*8u)return;
 uint64_t entries[ACPI806_MAX_ROOT]={0};size_t count=0;
 if(!acpi806_xsdt(xsdt,length,entries,&count))return;
 struct acpi806_topology result={0};unsigned found=0;
 for(size_t i=0;i<count;i++){
  const uint8_t *table=(const uint8_t *)(uintptr_t)entries[i];
  if(!sig4(table,"APIC"))continue;
  if(++found!=1)return;
  uint32_t n=le32(table+4);
  if(n<44||n>4096||!acpi806_madt(table,n,&result))return;
 }
 if(found!=1)return;
 result.root_entries=(uint32_t)count; result.revision=(uint32_t)rsdp[15];
 captured=result;ready=1;
}
int acpi806_copy_capture(struct acpi806_topology *out){if(!ready||!out)return 0;*out=captured;return 1;}
static _Noreturn void fail(const char *why){arch_puts("STYLEOS S806 FAIL ");arch_puts(why);arch_putc('\n');arch_halt();}
_Noreturn void acpi806_terminal(const struct boot_info *info){
 if(!info||info->magic!=STYLEOS_BOOT_MAGIC||info->flags!=STYLEOS_BOOT_EXITED_SERVICES)fail("handoff");
 if(!ready)fail("acpi-invalid-or-missing");
 const struct acpi806_topology *t=&captured;
 arch_puts("STYLEOS S806 ROOT");fld("revision",t->revision);fld("xsdt-entries",t->root_entries);fld("madt-length",t->madt_length);arch_putc('\n');
 arch_puts("STYLEOS S806 LAPIC");fld("physical",t->lapic_physical);fld("flags",t->madt_flags);fld("enabled-cpus",t->enabled_lapics);arch_putc('\n');
 for(uint32_t i=0;i<t->ioapic_count;i++){
  arch_puts("STYLEOS S806 IOAPIC");fld("index",i);fld("id",t->ioapic[i].id);fld("physical",t->ioapic[i].physical);fld("gsi-base",t->ioapic[i].gsi_base);arch_putc('\n');
 }
 for(uint32_t i=0;i<t->iso_count;i++){
  arch_puts("STYLEOS S806 OVERRIDE");fld("index",i);fld("bus",t->iso[i].bus);fld("source",t->iso[i].source);fld("gsi",t->iso[i].gsi);fld("flags",t->iso[i].flags);arch_putc('\n');
 }
 arch_puts("STYLEOS S806 SUMMARY");fld("ioapic-count",t->ioapic_count);fld("override-count",t->iso_count);arch_putc('\n');
 arch_puts("STYLEOS S806 POLICY acpi-checksums=verified madt-topology=observed ioapic-redirection-unverified=yes irq-delivery-unverified=yes dma-iommu-unverified=yes uefi-runtime-unverified=yes production-cr3=deny\n");
 arch_puts("STYLEOS S806 PASS\n");
 __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));
 arch_halt();
}
