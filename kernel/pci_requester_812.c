#include <styleos/pci_requester_812.h>
#include <styleos/dmar_808.h>
#include <styleos/iommu_platform_810.h>
#include <stdint.h>
#include <stddef.h>
static uint16_t rd16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
void pci812_topology_init(struct pci812_topology *t){
 if(!t)return;
 *t=(struct pci812_topology){0};t->seen[0]=1;t->upper[0]=255;t->buses=1;
}
/* Device addition only follows real PCI config reads. Secondary routes must
 * be unique, within an ancestor subordinate range, and never form a cycle. */
int pci812_add_function(struct pci812_topology *t,unsigned bus,unsigned dev,
 unsigned fun,unsigned kind,unsigned secondary,unsigned subordinate){
 if(!t||bus>255||dev>31||fun>7||!t->seen[bus]||
    t->functions>=PCI812_MAX_FUNCTIONS||(kind!=1&&kind!=2))return 0;
 unsigned rid=(bus<<8)|(dev<<3)|fun;
 if(t->kind[rid])return 0;
 if(kind==2){
  if(!secondary||secondary==bus||subordinate<secondary||
     subordinate>t->upper[bus]||t->seen[secondary]||
     t->bridges>=255||t->buses>=256)return 0;
 }
 t->kind[rid]=(uint8_t)kind;t->functions++;
 if(kind==2){
  t->seen[secondary]=1;t->upper[secondary]=(uint8_t)subordinate;
  t->next_bus[rid]=(uint8_t)secondary;t->bridges++;t->buses++;
 }
 return 1;
}
/* A path must follow actual bridges at every hop and end on the right kind.
 * This is topology evidence, not proof of IOMMU unit ownership. */
int pci812_resolve(const struct pci812_topology *t,unsigned bus,
 const uint8_t *pairs,size_t count,unsigned scope_kind,uint16_t *rid){
 if(!t||!pairs||!rid||!count||count>16||bus>255||!t->seen[bus]||
    (scope_kind!=1&&scope_kind!=2))return 0;
 for(size_t i=0;i<count;i++){
  unsigned dev=pairs[2*i],fun=pairs[2*i+1];
  if(dev>31||fun>7)return 0;
  unsigned id=(bus<<8)|(dev<<3)|fun;
  if(!t->kind[id])return 0;
  if(i+1<count){
   if(t->kind[id]!=2||!t->next_bus[id])return 0;
   bus=t->next_bus[id];if(!t->seen[bus])return 0;
  }else{
   if(t->kind[id]!=scope_kind)return 0;
   *rid=(uint16_t)id;return 1;
  }
 }
 return 0;
}
static int add_exact(struct pci812_audit *a,unsigned rid,unsigned unit,unsigned src){
 if(unit>=PCI812_MAX_UNITS)return 0;
 for(unsigned i=0;i<a->exact;i++)if(a->records[i].rid==rid){
  if(a->records[i].unit!=unit){a->collisions++;return 0;}
  return 1;
 }
 for(unsigned i=0;i<a->aliases;i++)if(a->alias[i].from==rid){
  a->collisions++;return 0;
 }
 if(a->exact>=PCI812_MAX_EXACT)return 0;
 a->records[a->exact++]=(struct pci812_entry){(uint16_t)rid,(uint8_t)unit,(uint8_t)src};
 return 1;
}
static int add_alias(struct pci812_audit *a,unsigned from,unsigned to,unsigned unit){
 if(unit>=PCI812_MAX_UNITS)return 0;
 for(unsigned i=0;i<a->exact;i++)if(a->records[i].rid==from){
  a->collisions++;return 0;
 }
 for(unsigned i=0;i<a->aliases;i++)if(a->alias[i].from==from){
  if(a->alias[i].to!=(uint16_t)to||a->alias[i].unit!=unit){
   a->collisions++;return 0;
  }
  return 1;
 }
 if(a->aliases>=PCI812_MAX_ALIASES)return 0;
 a->alias[a->aliases++]=(struct pci812_alias){(uint16_t)from,(uint16_t)to,(uint8_t)unit};
 return 1;
}
int pci812_lookup(const struct pci812_audit *a,uint16_t rid,uint8_t *unit){
 if(!a||!unit||a->collisions)return -1;
 for(unsigned i=0;i<a->exact;i++)if(a->records[i].rid==rid){
  *unit=a->records[i].unit;return 1;
 }
 return 0;
}
int pci812_allow_dma(const struct pci812_audit *a,uint16_t rid){
 (void)a;(void)rid;return 0;
}
int pci812_dmar_decode(const uint8_t *p,size_t len,
 const struct pci812_topology *t,struct pci812_audit *out){
 struct dmar808_info verified;
 if(!p||!t||!out||!dmar808_parse(p,len,&verified))return 0;
 struct pci812_audit a={0};
 for(size_t off=48;off<len;){
  if(len-off<4)return 0;
  unsigned type=rd16(p+off),sz=rd16(p+off+2);
  if(sz<4||sz>len-off)return 0;
  const uint8_t *v=p+off;
  if(type==0){
   if(sz<16||a.units>=PCI812_MAX_UNITS)return 0;
   unsigned unit=a.units++,seg=rd16(v+6);
   if(v[4]&1u)a.include_all++;
   if(seg)a.nonzero_segments++;
   for(size_t s=16;s<sz;){
    if(sz-s<8)return 0;
    const uint8_t *q=v+s;unsigned n=q[1],kind=q[0];
    if(n<8||n>sz-s||((n-6u)&1u))return 0;
    uint16_t rid=0;
    if(!seg&&(kind==1||kind==2)&&
       pci812_resolve(t,q[5],q+6,(n-6u)/2u,kind,&rid)){
     if(kind==1){if(!add_exact(&a,rid,unit,1))return 0;}
     else a.bridge_scopes++; /* no implicit downstream ownership */
    }else a.unresolved++;
    s+=n;
   }
  }else if(type==1){
   a.exceptions++; /* RMRR makes device memory exceptions possible */
  }else a.unresolved++;
  off+=sz;
 }
 if(a.units!=verified.drhd_count)return 0;
 *out=a;return 1;
}
int pci812_ivrs_decode(const uint8_t *p,size_t len,
 const struct pci812_topology *t,struct pci812_audit *out){
 struct iommu810_ivrs verified;
 if(!p||!t||!out||!ivrs810_parse(p,len,&verified))return 0;
 struct pci812_audit a={0};
 for(size_t off=48;off<len;){
  if(len-off<4)return 0;
  unsigned type=p[off],sz=rd16(p+off+2);
  if(sz<4||sz>len-off)return 0;
  if(type==0x10||type==0x11||type==0x40){
   unsigned fixed=type==0x10?24u:40u;
   if(sz<fixed||a.units>=PCI812_MAX_UNITS)return 0;
   unsigned unit=a.units++,seg=rd16(p+off+16);
   if(seg){a.nonzero_segments++;a.unresolved++;off+=sz;continue;}
   unsigned pending=0,start=0,target=0;
   for(size_t s=fixed;s<sz;){
    const uint8_t *q=p+off+s;unsigned ent=q[0],width;
    if(ent==0xf0){a.unresolved++;break;} /* variable HID, leave tail opaque */
    width=(ent&0x80u)?16u:((ent&0x40u)?8u:4u);
    if(width>sz-s)return 0;
    unsigned rid=rd16(q+1);
    if(pending){
     if(ent!=4||width!=4||rid<start)return 0;
     unsigned count=rid-start+1u;
     if(count>256u)a.unresolved++;
     else{
      for(unsigned id=start;id<=rid;id++){
       if(pending==2){if(!add_alias(&a,id,target,unit))return 0;}
       else if(!add_exact(&a,id,unit,pending==3?3u:2u))return 0;
      }
      a.ranges++;
     }
     pending=0;s+=width;continue;
    }
    if(ent==1&&width==4)a.include_all++;
    else if(ent==2&&width==4){if(!add_exact(&a,rid,unit,2))return 0;}
    else if(ent==3&&width==4){pending=1;start=rid;}
    else if(ent==0x42&&width==8){
     target=rd16(q+5);
     if(!add_alias(&a,rid,target,unit))return 0;
    }else if(ent==0x43&&width==8){
     pending=2;start=rid;target=rd16(q+5);
    }else if(ent==0x46&&width==8){
     a.extended++;if(!add_exact(&a,rid,unit,3))return 0;
    }else if(ent==0x47&&width==8){
     a.extended++;pending=3;start=rid;
    }else if(ent==0x48&&width==8){a.special++;a.unresolved++;}
    else if(ent==4&&width==4)return 0;
    else a.unresolved++;
    s+=width;
   }
   if(pending)return 0;
  }else if(type>=0x20&&type<=0x22)a.exceptions++;
  else a.unresolved++;
  off+=sz;
 }
 if(a.units!=verified.ivhd_count)return 0;
 *out=a;return 1;
}
