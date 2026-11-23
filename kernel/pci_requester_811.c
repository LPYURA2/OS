#include <styleos/pci_requester_811.h>
#include <styleos/dmar_808.h>
#include <styleos/iommu_platform_810.h>
#include <stdint.h>
#include <stddef.h>
static uint16_t r16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static int add(struct pci811_map *m,uint16_t rid,unsigned unit,unsigned kind){
 if(unit>=PCI811_MAX_UNITS||m->exact_count==PCI811_MAX_EXACT)return 0;
 for(unsigned i=0;i<m->exact_count;i++)if(m->entries[i].rid==rid){
  /* Duplicate same-unit scope is not a second proof; different units conflict. */
  if(m->entries[i].unit!=unit){m->collisions++;return 0;}
  return 1;
 }
 m->entries[m->exact_count++]=(struct pci811_exact){rid,(uint8_t)unit,(uint8_t)kind};return 1;
}
int pci811_lookup(const struct pci811_map *m,uint16_t rid,uint8_t *unit){
 if(!m||!unit||m->collisions)return -1;
 for(unsigned i=0;i<m->exact_count;i++)if(m->entries[i].rid==rid){*unit=m->entries[i].unit;return 1;}
 return 0;
}
int pci811_allow_dma(const struct pci811_map *m,uint16_t rid){(void)m;(void)rid;return 0;}
int pci811_dmar_decode(const uint8_t *p,size_t len,struct pci811_map *out){
 if(!out)return 0;
 struct dmar808_info validated;
 if(!dmar808_parse(p,len,&validated))return 0;
 struct pci811_map x={0};
 for(size_t off=48;off<len;){
  if(len-off<4)return 0;
  unsigned type=r16(p+off),size=r16(p+off+2);
  if(size<4||size>len-off)return 0;
  const uint8_t *v=p+off;
  if(type==0){
   if(size<16||x.units==PCI811_MAX_UNITS)return 0;
   unsigned unit=x.units++;
   if(v[4]&1u)x.include_all++;
   for(size_t s=16;s<size;){
    if(size-s<8)return 0;
    const uint8_t *q=v+s;unsigned sl=q[1],kind=q[0];
    if(sl<8||sl>size-s||((sl-6)&1u))return 0;
    /* Only a single-hop PCI endpoint scope can be decoded without a
     * verified PCI bridge hierarchy. Non-endpoints and multi-hop paths deny. */
    if(kind==1&&sl==8&&q[6]<32&&q[7]<8){
     unsigned rid=((unsigned)q[5]<<8)|((unsigned)q[6]<<3)|q[7];
     if(!add(&x,(uint16_t)rid,unit,1))return 0;
    }else x.unresolved++;
    s+=sl;
   }
  }else if(type!=1)x.unresolved++;
  off+=size;
 }
 if(x.units!=validated.drhd_count)return 0;
 *out=x;return 1;
}
int pci811_ivrs_decode(const uint8_t *p,size_t len,struct pci811_map *out){
 if(!out)return 0;
 struct iommu810_ivrs validated;
 if(!ivrs810_parse(p,len,&validated))return 0;
 struct pci811_map x={0};
 for(size_t off=48;off<len;){
  unsigned type=p[off],size=r16(p+off+2);
  if(size<4||size>len-off)return 0;
  if(type==0x10||type==0x11||type==0x40){
   if(x.units==PCI811_MAX_UNITS)return 0;
   unsigned unit=x.units++,fixed=(type==0x10?24:40);
   if(size<fixed)return 0;
   size_t s=fixed;int pending=0;uint16_t start=0;
   while(s<size){
    const uint8_t *q=p+off+s;
    unsigned ent=q[0],width=(ent&0x80)?16:((ent&0x40)?8:4);
    if(width>size-s)return 0;
    uint16_t rid=r16(q+1);
    if(pending){
     if(ent!=4||width!=4||rid<start)return 0;
     /* Bound expansion: a very broad interval is unresolved, never guessed. */
     if((unsigned)rid-(unsigned)start>255u)x.unresolved++;
     else {
      for(unsigned id=start;id<=rid;id++)if(!add(&x,(uint16_t)id,unit,2))return 0;
      x.ranges++;
     }
     pending=0;s+=width;continue;
    }
    if(ent==1&&width==4)x.include_all++;
    else if(ent==2&&width==4){if(!add(&x,rid,unit,2))return 0;}
    else if(ent==3&&width==4){pending=1;start=rid;}
    else if(ent==4&&width==4)return 0;
    else x.unresolved++;
    s+=width;
   }
   if(pending)return 0;
  }else if(type<0x20||type>0x22)x.unresolved++;
  off+=size;
 }
 if(x.units!=validated.ivhd_count)return 0;
 *out=x;return 1;
}
