#include <styleos/pci_rootbridge_814.h>
#include <stdint.h>
#include <stddef.h>
static uint16_t u16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint64_t u64(const uint8_t *p){uint64_t x=0;for(unsigned i=0;i<8;i++)x|=(uint64_t)p[i]<<(8*i);return x;}
int pci814_parse_configuration(uint32_t segment,const uint8_t *p,
 size_t len,struct pci814_root *out){
 if(!p||!out||len<2||len>PCI814_MAX_RESOURCES)return 0;
 struct pci814_root x={0};x.segment=segment;
 unsigned entries=0;size_t off=0;int ended=0;
 while(off<len){
  if(p[off]==0x79u){
   if(off+2u!=len||!x.bus_ranges)return 0;
   ended=1;break;
  }
  if(len-off<46||p[off]!=0x8au||u16(p+off+1)!=43u||
     ++entries>PCI814_MAX_DESCRIPTORS)return 0;
  const uint8_t *v=p+off;
  unsigned type=v[3],flags=v[4];
  uint64_t min=u64(v+14),max=u64(v+22),translation=u64(v+30),size=u64(v+38);
  if(type==2u){
   if(flags||translation||!size||min>255||max>255||min>max||
      size-1u>max-min||x.bus_ranges==UINT16_MAX)return 0;
   for(uint64_t b=min;b<min+size;b++){
    if(x.bus_present[b])return 0;
    x.bus_present[b]=1;
   }
   x.bus_ranges++;
  }else if(type==0u||type==1u){
   if(x.other_ranges==UINT16_MAX)return 0;
   x.other_ranges++;
  }else return 0;
  off+=46u;
 }
 if(!ended)return 0;
 *out=x;return 1;
}
int pci814_correlate(const struct pci814_root *roots,size_t count,
 const struct pci813_mcfg *m,const struct pci812_topology *t,
 struct pci814_report *out){
 if(!roots||!m||!t||!out||!count||count>PCI814_MAX_ROOTS||
    !t->buses||!t->functions||!t->seen[0])return 0;
 struct pci814_report x={0};x.roots=(uint16_t)count;
 const struct pci814_root *seg0=0;
 for(size_t i=0;i<count;i++){
  if(!roots[i].bus_ranges)return 0;
  if(roots[i].segment==0){x.segment0_roots++;seg0=&roots[i];}
  else x.other_segment_roots++;
  x.ranges+=roots[i].bus_ranges;
  for(size_t j=0;j<i;j++)if(roots[i].segment==roots[j].segment){
   for(unsigned b=0;b<256;b++)if(roots[i].bus_present[b]&&roots[j].bus_present[b])return 0;
  }
 }
 if(x.segment0_roots!=1||!seg0)return 0;
 for(unsigned bus=0;bus<256;bus++)if(t->seen[bus]){
  x.observed_buses++;
  uint64_t addr=0;
  if(!seg0->bus_present[bus]||pci813_ecam_address(m,0,bus,0,0,0,&addr)!=1)
   x.unresolved_buses++;
  else x.covered_buses++;
 }
 if(x.observed_buses!=t->buses||x.unresolved_buses||x.covered_buses!=t->buses)return 0;
 *out=x;return 1;
}
int pci814_allow_dma(unsigned seg,unsigned rid){(void)seg;(void)rid;return 0;}
