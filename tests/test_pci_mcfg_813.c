#include <styleos/pci_mcfg_813.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void w16(uint8_t *p,uint16_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void w32(uint8_t *p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(x>>(8*i));}
static void w64(uint8_t *p,uint64_t x){w32(p,(uint32_t)x);w32(p+4,(uint32_t)(x>>32));}
static void fix(uint8_t *p,size_t n){p[9]=0;unsigned sum=0;for(size_t i=0;i<n;i++)sum+=p[i];p[9]=(uint8_t)(0u-sum);}
static void fixture(uint8_t *p,size_t n){
 memset(p,0,128);memcpy(p,"MCFG",4);w32(p+4,(uint32_t)n);p[8]=1;
 w64(p+44,0xb0000000u);w16(p+52,0);p[54]=0;p[55]=15;
 if(n>=76){
  w64(p+60,0xc0000000u);w16(p+68,1);p[70]=16;p[71]=31;
 }
 fix(p,n);
}
static struct pci812_topology topo;
int main(void){
 uint8_t buf[128]={0};struct pci813_mcfg m={0},old;
 struct pci813_live live={0},saved;
 uint64_t pa=UINT64_C(0x1111111111111111);
 fixture(buf,76);assert(pci813_mcfg_parse(buf,76,&m));
 assert(m.count==2&&m.segment0_windows==1&&m.other_segment_windows==1);
 assert(m.windows[0].physical_first==0xb0000000u);
 assert(m.windows[0].physical_end==0xb1000000u);
 assert(m.windows[1].physical_first==0xc1000000u); /* Base means bus zero! */
 assert(m.windows[1].physical_end==0xc2000000u);
 assert(pci813_ecam_address(&m,0,2,3,1,0xabc,&pa)==1);
 assert(pa==UINT64_C(0xb0000000)+(UINT64_C(2)<<20)+(UINT64_C(3)<<15)+4096u+0xabcu);
 pa=42;assert(pci813_ecam_address(&m,1,16,0,0,0,&pa)==1&&pa==0xc1000000u);
 pa=42;assert(pci813_ecam_address(&m,1,15,0,0,0,&pa)==0&&pa==42);
 assert(pci813_ecam_address(&m,0,0,32,0,0,&pa)==-1&&pa==42);
 assert(pci813_ecam_address(&m,0,0,0,8,0,&pa)==-1&&pa==42);
 assert(pci813_ecam_address(&m,0,0,0,0,4096,&pa)==-1&&pa==42);
 assert(pci813_ecam_address(&m,65536,0,0,0,0,&pa)==-1&&pa==42);
 assert(!pci813_allow_dma(&m,0,0x11)&&!pci813_allow_dma(&m,1,0x20));
 pci812_topology_init(&topo);
 assert(pci812_add_function(&topo,0,1,0,2,2,7));
 assert(pci812_add_function(&topo,2,3,0,2,5,7));
 assert(pci812_add_function(&topo,5,2,1,1,0,0));
 assert(pci813_correlate_bus0(&m,&topo,&live));
 assert(live.discovered_buses==3&&live.covered_buses==3&&live.missing_buses==0);
 assert(live.root_base==0xb0000000u&&live.detected_functions==3&&live.detected_bridges==2);
 fixture(buf,60);assert(pci813_mcfg_parse(buf,60,&m));
 buf[55]=3;fix(buf,60);saved=live;assert(pci813_mcfg_parse(buf,60,&m));
 assert(!pci813_correlate_bus0(&m,&topo,&live)&&memcmp(&live,&saved,sizeof(live))==0);
 fixture(buf,76);assert(pci813_mcfg_parse(buf,76,&m));old=m;
 buf[9]^=1;assert(!pci813_mcfg_parse(buf,76,&m));assert(!memcmp(&m,&old,sizeof(m)));
 fixture(buf,76);buf[36]=1;fix(buf,76);assert(!pci813_mcfg_parse(buf,76,&m));
 fixture(buf,76);buf[59]=1;fix(buf,76);assert(!pci813_mcfg_parse(buf,76,&m));
 fixture(buf,76);buf[70]=34;buf[71]=30;fix(buf,76);assert(!pci813_mcfg_parse(buf,76,&m));
 fixture(buf,76);w64(buf+60,0xb0000000u);buf[70]=0;buf[71]=15;fix(buf,76);
 assert(!pci813_mcfg_parse(buf,76,&m)); /* overlapping PA and per-segment ranges */
 fixture(buf,76);w64(buf+60,0xaf000000u);fix(buf,76);
 assert(!pci813_mcfg_parse(buf,76,&m)); /* shared physical ECAM by other segment */
 fixture(buf,60);w64(buf+44,UINT64_C(0xfffffffffff00000));fix(buf,60);
 assert(!pci813_mcfg_parse(buf,60,&m)); /* arithmetic overflow */
 fixture(buf,60);w64(buf+44,0xb0000001u);fix(buf,60);
 assert(!pci813_mcfg_parse(buf,60,&m)); /* misalignment */
 fixture(buf,60);w32(buf+4,59);fix(buf,60);
 assert(!pci813_mcfg_parse(buf,60,&m));
 assert(!pci813_mcfg_parse(NULL,60,&m));
 assert(!pci813_mcfg_parse(buf,60,NULL));
 assert(!pci813_correlate_bus0(NULL,&topo,&live));
 assert(!pci813_correlate_bus0(&old,NULL,&live));
 puts("STYLEOS S813 HOST PASS MCFG checksums bounds multi-segment bus-zero-based ECAM and fail-closed");
 return 0;
}
