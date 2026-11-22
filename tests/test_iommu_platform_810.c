#include <styleos/iommu_platform_810.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static void u16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void u32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void u64(uint8_t *p,uint64_t v){u32(p,(uint32_t)v);u32(p+4,(uint32_t)(v>>32));}
static void fix(uint8_t *p,size_t n){p[9]=0;uint8_t s=0;for(size_t i=0;i<n;i++)s=(uint8_t)(s+p[i]);p[9]=(uint8_t)(0u-s);}
int main(void){
 uint8_t p[128]={0};struct iommu810_ivrs v={0};uint16_t id=0;
 memcpy(p,"IVRS",4);u32(p+4,72);p[8]=2;u32(p+36,0x12345678);
 p[48]=0x10;u16(p+50,24);u16(p+52,0x12);u16(p+54,0x40);u64(p+56,0xfed80000);u16(p+64,0);
 fix(p,72);assert(ivrs810_parse(p,72,&v));assert(v.ivhd_count==1&&v.units[0].mmio_base==0xfed80000);assert(v.units[0].requester_id==0x12);
 assert(!iommu810_allow_dma(IOMMU810_AMD,0x12));
 assert(iommu810_select(0,0,1,1)==IOMMU810_AMD);
 assert(iommu810_select(1,1,0,0)==IOMMU810_INTEL);
 assert(iommu810_select(0,0,0,0)==IOMMU810_ABSENT);
 assert(iommu810_select(1,1,1,1)==IOMMU810_REFUSED);
 assert(iommu810_select(1,0,0,0)==IOMMU810_REFUSED);
 assert(iommu810_select(0,0,1,0)==IOMMU810_REFUSED);
 struct iommu810_pci b={0,2,31,7};assert(iommu810_pci_requester(&b,&id)&&id==0x2ff);
 b.device=32;assert(!iommu810_pci_requester(&b,&id));b.device=31;b.function=8;assert(!iommu810_pci_requester(&b,&id));
 uint8_t q[128];memcpy(q,p,72);q[10]^=1;assert(!ivrs810_parse(q,72,&v));
 memcpy(q,p,72);u32(q+4,71);fix(q,72);assert(!ivrs810_parse(q,72,&v));
 memcpy(q,p,72);u16(q+50,0);fix(q,72);assert(!ivrs810_parse(q,72,&v));
 memcpy(q,p,72);u16(q+50,80);fix(q,72);assert(!ivrs810_parse(q,72,&v));
 memcpy(q,p,72);u64(q+56,0);fix(q,72);assert(!ivrs810_parse(q,72,&v));
 memcpy(q,p,72);u64(q+56,0xfed80001);fix(q,72);assert(!ivrs810_parse(q,72,&v));
 memcpy(q,p,72);q[48]=0x55;fix(q,72);assert(!ivrs810_parse(q,72,&v));
 memcpy(q,p,72);q[48]=0x10;u16(q+50,28);u32(q+4,76);fix(q,72);assert(!ivrs810_parse(q,72,&v));
 printf("STYLEOS S810 HOST PASS AMD-IVRS Intel/AMD selector PCI-BDF fail-closed\n");return 0;
}
