#include <styleos/pci_mcfg_813.h>
#include <stdint.h>
#include <stddef.h>

static uint16_t rd16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t rd32(const uint8_t *p){
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint64_t rd64(const uint8_t *p){return (uint64_t)rd32(p)|((uint64_t)rd32(p+4)<<32);}
static int checksum_ok(const uint8_t *p,size_t n){
 unsigned sum=0;for(size_t i=0;i<n;i++)sum=(sum+p[i])&255u;return sum==0;
}
int pci813_mcfg_parse(const uint8_t *p,size_t len,struct pci813_mcfg *out){
 if(!p||!out||len<60||len>PCI813_MAX_MCFG_BYTES||
    (len-44u)%16u||rd32(p+4)!=len||
    p[0]!='M'||p[1]!='C'||p[2]!='F'||p[3]!='G'||!checksum_ok(p,len))return 0;
 /* The fixed and per-allocation reserved bytes are mandatory zero here. */
 for(unsigned k=36;k<44;k++)if(p[k])return 0;
 size_t count=(len-44u)/16u;
 if(!count||count>PCI813_MAX_WINDOWS)return 0;
 struct pci813_mcfg tmp={0};
 for(size_t i=0;i<count;i++){
  const uint8_t *e=p+44u+i*16u;
  uint64_t base=rd64(e),lo=0,hi=0;
  unsigned seg=rd16(e+8),first=e[10],last=e[11];
  if(!base||(base&(PCI813_ECAM_BUS_BYTES-1u))||first>last||rd32(e+12))return 0;
  uint64_t low_offset=(uint64_t)first*PCI813_ECAM_BUS_BYTES;
  uint64_t high_offset=((uint64_t)last+1u)*PCI813_ECAM_BUS_BYTES;
  if(base>UINT64_MAX-high_offset)return 0;
  lo=base+low_offset;hi=base+high_offset;
  if(lo>=hi)return 0;
  for(unsigned j=0;j<tmp.count;j++){
   const struct pci813_window *w=&tmp.windows[j];
   if(w->segment==seg&&!(last<w->first_bus||first>w->last_bus))return 0;
   /* Even distinct PCI segments must never claim overlapping physical ECAM. */
   if(lo<w->physical_end&&w->physical_first<hi)return 0;
  }
  tmp.windows[tmp.count++]=(struct pci813_window){base,lo,hi,(uint16_t)seg,(uint8_t)first,(uint8_t)last};
  if(seg==0)tmp.segment0_windows++;else tmp.other_segment_windows++;
 }
 *out=tmp;return 1;
}
int pci813_ecam_address(const struct pci813_mcfg *m,unsigned seg,unsigned bus,
 unsigned dev,unsigned fun,unsigned reg,uint64_t *addr){
 if(!m||!addr||seg>65535u||bus>255u||dev>31u||fun>7u||reg>4095u||
    !m->count||m->count>PCI813_MAX_WINDOWS)return -1;
 const struct pci813_window *selected=0;
 for(unsigned i=0;i<m->count;i++){
  const struct pci813_window *w=&m->windows[i];
  if(w->segment!=seg||bus<w->first_bus||bus>w->last_bus)continue;
  if(selected)return -1;
  selected=w;
 }
 if(!selected)return 0;
 uint64_t offset=((uint64_t)bus<<20)|((uint64_t)dev<<15)|((uint64_t)fun<<12)|reg;
 /* Revalidate even a caller-supplied or corrupted parsed structure. */
 if(selected->base>UINT64_MAX-offset)return -1;
 uint64_t physical=selected->base+offset;
 if(physical<selected->physical_first||physical>=selected->physical_end)return -1;
 *addr=physical;return 1;
}
int pci813_correlate_bus0(const struct pci813_mcfg *m,
 const struct pci812_topology *t,struct pci813_live *out){
 if(!m||!t||!out||!t->seen[0]||!t->buses||!t->functions||
    t->functions>PCI812_MAX_FUNCTIONS)return 0;
 struct pci813_live tmp={0};
 tmp.detected_functions=t->functions;tmp.detected_bridges=t->bridges;
 for(unsigned bus=0;bus<256;bus++)if(t->seen[bus]){
  tmp.discovered_buses++;
  uint64_t pa=0;int match=pci813_ecam_address(m,0,bus,0,0,0,&pa);
  if(match<0)return 0;
  if(match==0)tmp.missing_buses++;
  else{
   tmp.covered_buses++;
   if(bus==0){
    tmp.root_base=pa;
    /* Root bus 0 end exclusive; not a verified mapping. */
    tmp.root_end=pa+PCI813_ECAM_BUS_BYTES;
   }
  }
 }
 if(tmp.discovered_buses!=t->buses||tmp.discovered_buses!=tmp.covered_buses+tmp.missing_buses||
    tmp.detected_bridges+1u!=t->buses||!tmp.root_base||tmp.missing_buses)return 0;
 *out=tmp;return 1;
}
int pci813_allow_dma(const struct pci813_mcfg *m,unsigned seg,unsigned rid){
 (void)m;(void)seg;(void)rid;return 0;
}
