#include "efi.h"
#include <styleos/pci_requester_812.h>
#include <styleos/pci_mcfg_813.h>
#include <styleos/pci_rootbridge_814.h>
#include <styleos/acpi_hostbridge_815.h>
#include <styleos/dmar_808.h>
#include <styleos/iommu_platform_810.h>
#include <styleos/acpi_madt_806.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
/* Terminal S813 only. ACPI MCFG and requester tables are validated and copied
 * into fixed image-owned BSS BEFORE EBS. After EBS only legacy CF8/CFC PCI
 * config reads are performed; computed ECAM addresses are NEVER dereferenced. */
struct cfg813 {struct efi_guid guid;void *table;};
static struct pci812_topology topo;
static struct pci812_audit audit;
static struct pci813_mcfg mcfg813;
static struct pci813_live live813;
static uint8_t mcfg_copy[PCI813_MAX_MCFG_BYTES];
static uint32_t mcfg_size;
static unsigned mcfg_seen;
static uint8_t dmar_copy[DMAR808_MAX_TABLE],ivrs_copy[IOMMU810_MAX_TABLE];
static uint32_t dmar_size,ivrs_size,root_count,saved_cf8;
static enum iommu810_backend backend=IOMMU810_REFUSED;
static int valid;
static uint8_t dsdt_copy[ACPI815_MAX_DSDT];
static uint32_t dsdt_length;
static unsigned fadt_seen,ssdt_seen;
static struct acpi815_report aml815;
/* A separate image-owned copy of UEFI RootBridgeIo resources, never AML. */
static struct pci814_root roots[PCI814_MAX_ROOTS];
static struct pci814_report root_report;
static uint8_t resources_copy[PCI814_MAX_ROOTS][PCI814_MAX_RESOURCES];
static uint16_t resources_len[PCI814_MAX_ROOTS];
static uint32_t resources_segment[PCI814_MAX_ROOTS];
static unsigned bridge_count;
static int root_capture_ok;
typedef EFI_STATUS (EFIAPI *locate_handle814)(unsigned,struct efi_guid *,void *,UINTN *,EFI_HANDLE *);
struct bridge_io814 {
 EFI_HANDLE parent;
 void *poll_mem,*poll_io;
 struct {void *read,*write;} mem,io,pci;
 void *copy_mem,*map,*unmap,*allocate_buffer,*free_buffer,*flush,*get_attributes,*set_attributes;
 EFI_STATUS (EFIAPI *configuration)(struct bridge_io814 *,void **);
 uint32_t segment;
};
_Static_assert(offsetof(struct bridge_io814,configuration)==136u &&
               offsetof(struct bridge_io814,segment)==144u,
               "UEFI Root Bridge I/O protocol layout");
/* LocateHandle(ByProtocol) and Configuration are called BEFORE EBS only;
 * no Map/Unmap/DMA/config-space protocol operations are invoked. */
static int capture_bridges(const struct efi_system_table *st){
 static const struct efi_guid g={0x2f707ebb,0x4a1a,0x11d4,
  {0x9a,0x38,0x00,0x90,0x27,0x3f,0xc1,0x4d}};
 if(!st||!st->boot_services||!st->boot_services->locate_handle||
    !st->boot_services->handle_protocol)return 0;
 locate_handle814 locate=(locate_handle814)st->boot_services->locate_handle;
 EFI_HANDLE handles[PCI814_MAX_ROOTS]={0};
 UINTN bytes=sizeof handles;
 if(locate(2u,(struct efi_guid *)&g,0,&bytes,handles)!=EFI_SUCCESS||
    bytes==0||bytes%sizeof(EFI_HANDLE)||bytes>sizeof handles)return 0;
 bridge_count=(unsigned)(bytes/sizeof(EFI_HANDLE));
 for(unsigned i=0;i<bridge_count;i++){
  void *interface=0,*resources=0;
  if(!handles[i]||st->boot_services->handle_protocol(handles[i],
     (struct efi_guid *)&g,&interface)!=EFI_SUCCESS||!interface)return 0;
  struct bridge_io814 *r=(struct bridge_io814 *)interface;
  if(!r->configuration||r->segment>65535u||
     r->configuration(r,&resources)!=EFI_SUCCESS||!resources)return 0;
  const uint8_t *p=(const uint8_t *)resources;
  unsigned off=0,entries=0;
  while(off+2u<=PCI814_MAX_RESOURCES){
   if(p[off]==0x79u){
    resources_copy[i][off]=p[off];
    resources_copy[i][off+1u]=p[off+1u];
    off+=2u;break;
   }
   if(off+46u>PCI814_MAX_RESOURCES||p[off]!=0x8au||
      p[off+1]!=43u||p[off+2]!=0u||
      ++entries>PCI814_MAX_DESCRIPTORS)return 0;
   for(unsigned k=0;k<46u;k++)resources_copy[i][off+k]=p[off+k];
   off+=46u;
  }
  if(off<2u||off>PCI814_MAX_RESOURCES||
     resources_copy[i][off-2u]!=0x79u)return 0;
  resources_len[i]=(uint16_t)off;
  resources_segment[i]=r->segment;
 }
 return 1;
}
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
void pci815_capture(const struct efi_system_table *st){
 static const struct efi_guid guid={
  0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
 valid=0;backend=IOMMU810_REFUSED;root_count=0;dmar_size=0;ivrs_size=0;mcfg_size=0;mcfg_seen=0;
 dsdt_length=0;fadt_seen=0;ssdt_seen=0;
 root_capture_ok=capture_bridges(st);
 if(!st||!st->configuration_table||!st->number_of_table_entries||
    st->number_of_table_entries>256)return;
 const struct cfg813 *c=st->configuration_table;const uint8_t *rsdp=0;
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
  if(p[0]=='F'&&p[1]=='A'&&p[2]=='C'&&p[3]=='P'){
   if(fadt_seen++)return;
   unsigned size=u32(p+4);uint64_t dsdt_address=0;
   if(!acpi815_fadt_dsdt(p,size,&dsdt_address))return;
   const uint8_t *dsdt=(const uint8_t *)(uintptr_t)dsdt_address;
   if(dsdt[0]!='D'||dsdt[1]!='S'||dsdt[2]!='D'||dsdt[3]!='T')return;
   unsigned length=u32(dsdt+4);
   if(length<37u||length>ACPI815_MAX_DSDT)return;
   uint8_t checksum=0;
   for(unsigned k=0;k<length;k++){
    uint8_t b=dsdt[k];checksum=(uint8_t)(checksum+b);dsdt_copy[k]=b;
   }
   if(checksum)return;
   dsdt_length=length;
  }else if(p[0]=='S'&&p[1]=='S'&&p[2]=='D'&&p[3]=='T'){
   if(++ssdt_seen>ACPI815_MAX_SSDT)return;
  }else if(p[0]=='M'&&p[1]=='C'&&p[2]=='F'&&p[3]=='G'){
   if(mcfg_seen++)return;
   unsigned size=u32(p+4);struct pci813_mcfg candidate={0};
   if(size>=60u&&size<=PCI813_MAX_MCFG_BYTES&&pci813_mcfg_parse(p,size,&candidate)){
    for(unsigned k=0;k<size;k++)mcfg_copy[k]=p[k];
    mcfg_size=size;
   }
  }else if(p[0]=='D'&&p[1]=='M'&&p[2]=='A'&&p[3]=='R'){
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
 valid=(backend!=IOMMU810_REFUSED&&mcfg_seen==1u&&mcfg_size!=0u&&fadt_seen==1u&&dsdt_length!=0u);
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
 arch_puts("STYLEOS S815 FAIL ");arch_puts(reason);arch_putc('\n');arch_halt();
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
_Noreturn void pci815_terminal(const struct boot_info *boot){
 if(!boot||boot->magic!=STYLEOS_BOOT_MAGIC||
    boot->flags!=STYLEOS_BOOT_EXITED_SERVICES)die("handoff");
 if(!valid||backend==IOMMU810_REFUSED)die("acpi");
 if(!root_capture_ok||!bridge_count)die("rootbridge-protocol");
 for(unsigned r=0;r<bridge_count;r++)
  if(!pci814_parse_configuration(resources_segment[r],resources_copy[r],
     resources_len[r],&roots[r]))die("rootbridge-resources");
 if(!pci813_mcfg_parse(mcfg_copy,mcfg_size,&mcfg813))die("mcfg");
 if(!acpi815_inspect(dsdt_copy,dsdt_length,&mcfg813,ssdt_seen,&aml815))die("dsdt-aml");
 scan_pci();
 if(!pci813_correlate_bus0(&mcfg813,&topo,&live813))die("ecam-bus-coverage");
 if(!pci814_correlate(roots,bridge_count,&mcfg813,&topo,&root_report))
  die("rootbridge-bus-coverage");
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
 arch_puts("STYLEOS S815 PLATFORM");f("backend",(uint64_t)backend);f("root-entries",root_count);arch_putc('\n');
 arch_puts("STYLEOS S815 TOPOLOGY");f("buses",topo.buses);f("functions",topo.functions);
 f("bridges",topo.bridges);arch_putc('\n');
 arch_puts("STYLEOS S815 MCFG");f("allocations",mcfg813.count);
 f("segment0",mcfg813.segment0_windows);f("other-segments",mcfg813.other_segment_windows);
 f("covered-buses",live813.covered_buses);f("missing-buses",live813.missing_buses);
 f("root-base",live813.root_base);f("root-end",live813.root_end);arch_putc('\n');
 arch_puts("STYLEOS S815 SCOPE");f("units",audit.units);f("exact",audit.exact);
 f("unresolved",audit.unresolved);f("aliases",audit.aliases);f("exceptions",audit.exceptions);
 f("segments-unresolved",audit.nonzero_segments);f("collisions",audit.collisions);arch_putc('\n');
 arch_puts("STYLEOS S815 PCI");f("explicit-matches",matched);f("unresolved",unknown);
 f("alias-sources-present",alias_present);arch_putc('\n');
 if(!topo.functions||!live813.covered_buses||live813.covered_buses!=topo.buses||
    matched>topo.functions||matched+unknown!=topo.functions||
    alias_present>topo.functions)die("pci-mismatch");
 arch_puts("STYLEOS S815 ROOTBRIDGE");f("roots",root_report.roots);
 f("segment0",root_report.segment0_roots);f("other-segments",root_report.other_segment_roots);
 f("bus-ranges",root_report.ranges);f("covered-buses",root_report.covered_buses);
 f("unresolved-buses",root_report.unresolved_buses);arch_putc('\n');
 arch_puts("STYLEOS S815 AML");f("dsdt-bytes",aml815.dsdt_length);
 f("devices",aml815.aml_device_packages);f("pci-hid",aml815.pci_hid_candidates);
 f("motherboard-hid",aml815.motherboard_hid_candidates);
 f("seg-static",aml815.seg_static_candidates);f("bbn-static",aml815.bbn_static_candidates);
 f("crs-static",aml815.crs_static_candidates);f("crs-method",aml815.crs_method_candidates);
 f("ecam-literal",aml815.ecam_literal_matches);f("ssdts",aml815.ssdt_count);
 arch_putc('\n');
 for(unsigned off=0;off<dsdt_length;off+=32u){
  arch_puts("STYLEOS S815 DSDTHEX");f("offset",off);arch_puts(" data=");
  unsigned n=dsdt_length-off;if(n>32)n=32;
  for(unsigned i=0;i<n;i++){
   uint8_t b=dsdt_copy[off+i];
   arch_putc("0123456789abcdef"[b>>4]);arch_putc("0123456789abcdef"[b&15]);
  }
  arch_putc('\n');
 }
 arch_puts("STYLEOS S815 POLICY acpi-dsdt=checksummed acpi-aml=static-hints-only acpi-crs-evaluated=no acpi-seg-evaluated=no acpi-bbn-evaluated=no ecam-reservation-verified=no ecam-dereference=no iommu-ownership-complete=no dma-domains-verified=no dma-access-authorized=no dma-isolation-verified=no production-cr3=deny\n");
 arch_puts("STYLEOS S815 PASS\n");
 port_out(0xf4u,0x10u);arch_halt();
}
