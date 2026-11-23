#include <styleos/acpi_hostbridge_815.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static uint8_t dsdt[512],fadt[200];
static void wr32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8u*i));}
static void wr64(uint8_t *p,uint64_t v){wr32(p,(uint32_t)v);wr32(p+4,(uint32_t)(v>>32));}
static void fix(uint8_t *p,size_t n){p[9]=0;uint8_t sum=0;for(size_t i=0;i<n;i++)sum=(uint8_t)(sum+p[i]);p[9]=(uint8_t)(0u-sum);}
static size_t make_dsdt(void){
 memset(dsdt,0,sizeof dsdt);memcpy(dsdt,"DSDT",4);dsdt[8]=2;
 size_t i=36;
 dsdt[i++]=0x5b;dsdt[i++]=0x82;size_t pci_pkg=i++;
 memcpy(dsdt+i,"PCI0",4);i+=4;
 static const uint8_t hid[]={0x08,'_','H','I','D',0x0c,0x41,0xd0,0x0a,0x08};
 memcpy(dsdt+i,hid,sizeof hid);i+=sizeof hid;
 static const uint8_t seg[]={0x08,'_','S','E','G',0x00,0x08,'_','B','B','N',0x00};
 memcpy(dsdt+i,seg,sizeof seg);i+=sizeof seg;
 static const uint8_t crs[]={0x14,0x06,'_','C','R','S',0x00};
 memcpy(dsdt+i,crs,sizeof crs);i+=sizeof crs;
 dsdt[pci_pkg]=(uint8_t)(i-pci_pkg);
 dsdt[i++]=0x5b;dsdt[i++]=0x82;size_t board_pkg=i++;
 memcpy(dsdt+i,"CRES",4);i+=4;
 static const uint8_t board_hid[]={0x08,'_','H','I','D',0x0c,0x41,0xd0,0x0c,0x02};
 memcpy(dsdt+i,board_hid,sizeof board_hid);i+=sizeof board_hid;
 static const uint8_t prefix[]={0x08,'_','C','R','S',0x11,0x11,0x0a,0x0e};
 memcpy(dsdt+i,prefix,sizeof prefix);i+=sizeof prefix;
 dsdt[i++]=0x86;dsdt[i++]=0x09;dsdt[i++]=0;dsdt[i++]=0;
 wr32(dsdt+i,0xe0000000u);i+=4;wr32(dsdt+i,0x10000000u);i+=4;
 dsdt[i++]=0x79;dsdt[i++]=0;
 dsdt[board_pkg]=(uint8_t)(i-board_pkg);
 wr32(dsdt+4,(uint32_t)i);fix(dsdt,i);return i;
}
static void make_fadt(void){
 memset(fadt,0,sizeof fadt);memcpy(fadt,"FACP",4);wr32(fadt+4,148);
 wr32(fadt+40,0x12340000u);wr64(fadt+140,0x12340000u);fix(fadt,148);
}
int main(void){
 make_fadt();uint64_t addr=123;
 assert(acpi815_fadt_dsdt(fadt,148,&addr)&&addr==0x12340000u);
 uint64_t old=addr;fadt[9]^=1;
 assert(!acpi815_fadt_dsdt(fadt,148,&addr)&&addr==old);fadt[9]^=1;
 wr64(fadt+140,0x12345000u);fix(fadt,148);
 assert(!acpi815_fadt_dsdt(fadt,148,&addr)&&addr==old);
 make_fadt();wr64(fadt+140,0);fix(fadt,148);
 assert(acpi815_fadt_dsdt(fadt,148,&addr)&&addr==0x12340000u);
 size_t size=make_dsdt();
 struct pci813_mcfg m={0};m.count=1;m.segment0_windows=1;
 m.windows[0]=(struct pci813_window){UINT64_C(0xe0000000),UINT64_C(0xe0000000),UINT64_C(0xf0000000),0,0,255};
 struct acpi815_report r={0};assert(acpi815_inspect(dsdt,size,&m,0,&r));
 assert(r.aml_device_packages==2&&r.pci_hid_candidates==1&&r.motherboard_hid_candidates==1);
 assert(r.seg_static_candidates==1&&r.bbn_static_candidates==1);
 assert(r.segment0_static_observed&&r.bus0_static_observed&&r.crs_method_candidates==1);
 assert(r.crs_static_candidates==1&&r.ecam_literal_matches==1);
 assert(!r.acpi_namespace_evaluated&&!r.crs_evaluated&&!r.iommu_owned);
 assert(!acpi815_allow_dma(&r,0));
 struct acpi815_report snapshot=r;
 dsdt[9]^=1;assert(!acpi815_inspect(dsdt,size,&m,0,&r));
 assert(!memcmp(&r,&snapshot,sizeof r));dsdt[9]^=1;
 dsdt[37]=0x7f;fix(dsdt,size);
 assert(acpi815_inspect(dsdt,size,&m,1,&r)&&r.aml_device_packages==1);
 size=make_dsdt();m.windows[0].physical_first=UINT64_C(0xd0000000);
 assert(acpi815_inspect(dsdt,size,&m,0,&r)&&r.ecam_literal_matches==0);
 m.windows[0].physical_first=UINT64_C(0xe0000000);
 assert(!acpi815_inspect(dsdt,size,&m,65,&r));
 assert(!acpi815_inspect(0,0,&m,0,&r));
 assert(!acpi815_fadt_dsdt(0,0,&addr));
 puts("STYLEOS S815 HOST PASS FADT DSDT bounded static AML hints, non-evaluation and DMA deny");
 return 0;
}
