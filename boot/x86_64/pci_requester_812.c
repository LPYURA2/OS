#include "efi.h"
#include <styleos/pci_requester_812.h>
#include <styleos/dmar_808.h>
#include <styleos/iommu_platform_810.h>
#include <styleos/acpi_madt_806.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
/* Terminal S812 only. ACPI tables are validated and copied into image-owned
 * storage before ExitBootServices; PCI configuration is read after EBS. */
struct cfg812 {struct efi_guid guid;void *table;};
static struct pci812_topology topo;
static struct pci812_audit audit;
static uint8_t dmar_copy[DMAR808_MAX_TABLE],ivrs_copy[IOMMU810_MAX_TABLE];
static uint32_t dmar_size,ivrs_size,root_count,saved_cf8;
static enum iommu810_backend backend=IOMMU810_REFUSED;
static int valid;
static int guid_eq(const struct efi_guid *a,const struct efi_guid *b){
 if(a->a!=b->a||a->b!=b->b||a->c!=b->c)return 0;
 for(unsigned i=0;i<8;i++)if(a->d[i]!=b->d[i])return 0;
 return 1;
}
static uint32_t u32(const uint8_t *p){
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void h(uint64_t n){
 arch_puts("0x");for(int i=60;i>=0;i-=4)
  arch_putc("0123456789abcdef"[(n>>(unsigned)i)&15u]);
}
static void f(const char *key,uint64_t n){
 arch_putc(' ');arch_puts(key);arch_putc('=');h(n);
}
void pci812_capture(const struct efi_system_table *st){
 static const struct efi_guid guid={
  0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
 valid=0;backend=IOMMU810_REFUSED;root_count=0;dmar_size=0;ivrs_size=0;
 if(!st||!st->configuration_table||!st->number_of_table_entries||
    st->number_of_table_entries>256)return;
 const struct cfg812 *c=st->configuration_table;const uint8_t *rsdp=0;
 for(size_t i=0;i<st->number_of_table_entries;i++)if(guid_eq(&c[i].guid,&guid)){
  if(rsdp)return;rsdp=(const uint8_t *)c[i].table;
 }
 uint64_t addr=0;
 if(!rsdp||!acpi806_rsdp(rsdp,36,&addr))return;
 const uint8_t *xsdt=(const uint8_t *)(uintptr_t)addr;
 uint32_t length=u32(xsdt+4);
 if(length<44||length>36+ACPI806_MAX_ROOT*8u)return;
 uint64_t entries[ACPI806_MAX_ROOT]={0};size_t n=0;
 if(!acpi806_xsdt(xsdt,length,entries,&n))return;
 root_count=(uint32_t)n;
 int dm=0,iv=0,dv=0,ivv=0;
 for(size_t i=0;i<n;i++){
  const uint8_t *p=(const uint8_t *)(uintptr_t)entries[i];
  if(p[0]=='D'&&p[1]=='M'&&p[2]=='A'&&p[3]=='R'){
   if(dm++)return;
   unsigned size=u32(p+4);struct dmar808_info d={0};
   if(size>=48&&size<=DMAR808_MAX_TABLE&&dmar808_parse(p,size,&d)){
    for(unsigned k=0;k<size;k++)dmar_copy[k]=p[k];
    dmar_size=size;dv=1;
   }
  }else if(p[0]=='I'&&p[1]=='V'&&p[2]=='R'&&p[3]=='S'){
   if(iv++)return;
   unsigned size=u32(p+4);struct iommu810_ivrs v={0};
   if(size>=48&&size<=IOMMU810_MAX_TABLE&&ivrs810_parse(p,size,&v)){
    for(unsigned k=0;k<size;k++)ivrs_copy[k]=p[k];
    ivrs_size=size;ivv=1;
   }
  }
 }
 backend=iommu810_select(dm,dv,iv,ivv);
 valid=(backend!=IOMMU810_REFUSED);
}
static uint32_t port_in(uint16_t port){
 uint32_t out;__asm__ volatile("inl %%dx,%0":"=a"(out):"d"(port):"memory");return out;
}
static void port_out(uint16_t port,uint32_t v){
 __asm__ volatile("outl %0,%%dx"::"a"(v),"d"(port):"memory");
}
static uint32_t config(unsigned bus,unsigned dev,unsigned fn,unsigned off){
 uint32_t addr=UINT32_C(0x80000000)|((uint32_t)bus<<16)|
  ((uint32_t)dev<<11)|((uint32_t)fn<<8)|(uint32_t)(off&0xfcu);
 port_out(0xcf8u,addr);return port_in(0xcfcu);
}
static _Noreturn void die(const char *reason){
 port_out(0xcf8u,saved_cf8);
 arch_puts("STYLEOS S812 FAIL ");arch_puts(reason);arch_putc('\n');arch_halt();
}
static void scan_pci(void){
 uint8_t queue[256]={0};unsigned head=0,tail=1;
 pci812_topology_init(&topo);
 queue[0]=0;saved_cf8=port_in(0xcf8u);
 while(head<tail){
  unsigned bus=queue[head++];
  for(unsigned dev=0;dev<32;dev++)for(unsigned fn=0;fn<8;fn++){
   uint32_t id=config(bus,dev,fn,0);
   if((id&0xffffu)==0xffffu||(id&0xffffu)==0)continue;
   uint32_t cc=config(bus,dev,fn,8),hdr=config(bus,dev,fn,12);
   unsigned type=(hdr>>16)&0x7fu;
   if(type>1)return die("unknown-header");
   unsigned sec=0,sub=0;
   if(type==1){
    if((cc>>24)!=6u||((cc>>16)&0xffu)!=4u)die("non-pci-bridge");
    uint32_t buses=config(bus,dev,fn,24);
    if((buses&0xffu)!=bus)die("bad-primary");
    sec=(buses>>8)&0xffu;sub=(buses>>16)&0xffu;
   }
   if(!pci812_add_function(&topo,bus,dev,fn,type==1?2u:1u,sec,sub))
    die("duplicate-or-invalid-route");
   if(type==1){
    if(tail>=256u)die("too-many-buses");
    queue[tail++]=(uint8_t)sec;
   }
  }
 }
 port_out(0xcf8u,saved_cf8);
}
_Noreturn void pci812_terminal(const struct boot_info *boot){
 if(!boot||boot->magic!=STYLEOS_BOOT_MAGIC||
    boot->flags!=STYLEOS_BOOT_EXITED_SERVICES)die("handoff");
 if(!valid||backend==IOMMU810_REFUSED)die("acpi");
 scan_pci();
 int parsed=1;
 if(backend==IOMMU810_INTEL)parsed=pci812_dmar_decode(dmar_copy,dmar_size,&topo,&audit);
 else if(backend==IOMMU810_AMD)parsed=pci812_ivrs_decode(ivrs_copy,ivrs_size,&topo,&audit);
 else audit=(struct pci812_audit){0};
 if(!parsed||audit.collisions)die("requester-scopes");
 unsigned matched=0,unknown=0,alias_present=0;
 for(unsigned rid=0;rid<65536;rid++)if(topo.kind[rid]){
  uint8_t unit=0;int found=pci812_lookup(&audit,(uint16_t)rid,&unit);
  if(found==1)matched++;else unknown++;
  for(unsigned j=0;j<audit.aliases;j++)if(audit.alias[j].from==rid)alias_present++;
 }
 arch_puts("STYLEOS S812 PLATFORM");f("backend",(uint64_t)backend);f("root-entries",root_count);arch_putc('\n');
 arch_puts("STYLEOS S812 TOPOLOGY");f("buses",topo.buses);f("functions",topo.functions);
 f("bridges",topo.bridges);arch_putc('\n');
 arch_puts("STYLEOS S812 SCOPE");f("units",audit.units);f("exact",audit.exact);
 f("unresolved",audit.unresolved);f("bridge-scopes",audit.bridge_scopes);
 f("aliases",audit.aliases);f("extended",audit.extended);
 f("include-all",audit.include_all);f("exceptions",audit.exceptions);
 f("segments-unresolved",audit.nonzero_segments);f("collisions",audit.collisions);
 arch_putc('\n');
 arch_puts("STYLEOS S812 PCI");f("explicit-matches",matched);f("unresolved",unknown);
 f("alias-sources-present",alias_present);arch_putc('\n');
 if(!topo.functions||matched>topo.functions||matched+unknown!=topo.functions||
    alias_present>topo.functions)die("pci-mismatch");
 arch_puts("STYLEOS S812 POLICY pci-bridge-walk=bounded multihop-intel=topology-checked amd-alias=not-ownership exceptions=not-grants segments=zero-only device-ownership-complete=no dma-domains-verified=no dma-access-authorized=no dma-isolation-verified=no production-cr3=deny\n");
 arch_puts("STYLEOS S812 PASS\n");
 port_out(0xf4u,0x10u);arch_halt();
}
