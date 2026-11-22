#include <styleos/iommu_platform_810.h>
#include <stdint.h>
#include <stddef.h>
static uint16_t rd16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t rd64(const uint8_t *p){return (uint64_t)rd32(p)|((uint64_t)rd32(p+4)<<32);}
static int sum0(const uint8_t *p,size_t n){uint8_t s=0;for(size_t i=0;i<n;i++)s=(uint8_t)(s+p[i]);return s==0;}
int ivrs810_parse(const uint8_t *p,size_t len,struct iommu810_ivrs *out){
 if(!p||!out||len<48||len>IOMMU810_MAX_TABLE)return 0;
 if(p[0]!='I'||p[1]!='V'||p[2]!='R'||p[3]!='S'||rd32(p+4)!=len||!sum0(p,len))return 0;
 /* IVRS starts with the 36-byte standard ACPI header, IVinfo, reserved 8 bytes. */
 struct iommu810_ivrs x={0};x.length=(uint32_t)len;x.revision=p[8];x.ivinfo=rd32(p+36);
 for(size_t off=48;off<len;){
  if(len-off<4)return 0;
  uint8_t type=p[off];size_t size=rd16(p+off+2);
  if(size<4||size>len-off)return 0;
  if(type==0x10||type==0x11||type==0x40){
   size_t fixed=(type==0x10?24:40);
   if(size<fixed||x.ivhd_count==IOMMU810_MAX_UNITS)return 0;
   uint64_t base=rd64(p+off+8);
   uint16_t seg=rd16(p+off+16),requester=rd16(p+off+4);
   if(!base||(base&4095u)||base>UINT64_MAX-4095u)return 0;
   for(size_t j=0;j<x.ivhd_count;j++)if(x.units[j].mmio_base==base&&x.units[j].type==type)return 0;
   /* IVHD may contain multiple 4/8/variable-length PCI entries: no fake
    * scope resolution. Keep an opaque count and deny all DMA requests. */
   if(size>fixed){if(x.opaque_entries==UINT16_MAX)return 0;x.opaque_entries++;}
   x.units[x.ivhd_count++]=(struct iommu810_unit){base,seg,requester,type};
  }else if(type>=0x20&&type<=0x22){
   if(size<32||x.ivmd_count==UINT16_MAX)return 0;
   uint64_t base=rd64(p+off+16),extent=rd64(p+off+24);
   if(!extent||base>UINT64_MAX-extent)return 0;
   x.ivmd_count++;
  }else{
   if(x.unknown_blocks==UINT16_MAX)return 0;
   x.unknown_blocks++;
  }
  off+=size;
 }
 if(!x.ivhd_count)return 0;
 *out=x;return 1;
}
enum iommu810_backend iommu810_select(int dm,int dv,int iv,int ivv){
 if((dm&&!dv)||(iv&&!ivv)||(dm&&iv))return IOMMU810_REFUSED;
 if(dm)return IOMMU810_INTEL;
 if(iv)return IOMMU810_AMD;
 return IOMMU810_ABSENT;
}
int iommu810_pci_requester(const struct iommu810_pci *pci,uint16_t *id){
 if(!pci||!id||pci->device>31||pci->function>7)return 0;
 *id=(uint16_t)(((uint16_t)pci->bus<<8)|((uint16_t)pci->device<<3)|pci->function);
 return 1;
}
/* Discovery is NOT DMA authorization. There is no domain map in S810. */
int iommu810_allow_dma(enum iommu810_backend vendor,uint16_t requester_id){
 (void)vendor;(void)requester_id;return 0;
}
