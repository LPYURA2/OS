#include <styleos/pci_rootbridge_814.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static uint8_t buffer[PCI814_MAX_RESOURCES];
static struct pci812_topology topo;
static void wr64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(i*8));}
static void resource(uint8_t *p,unsigned first,unsigned last){
 memset(p,0,46);p[0]=0x8a;p[1]=43;p[3]=2;
 wr64(p+14,first);wr64(p+22,last);wr64(p+38,last-first+1u);
}
static void prepare(struct pci814_root *r){
 memset(buffer,0,sizeof buffer);resource(buffer,0,255);
 buffer[46]=0x79;buffer[47]=0;
 assert(pci814_parse_configuration(0,buffer,48,r));
}
int main(void){
 struct pci814_root root={0},unchanged={0};
 struct pci813_mcfg m={0};struct pci814_report report={0},oldreport={0};
 pci812_topology_init(&topo);
 assert(pci812_add_function(&topo,0,1,0,2,2,7));
 assert(pci812_add_function(&topo,2,3,0,2,5,7));
 assert(pci812_add_function(&topo,5,2,0,1,0,0));
 m.count=1;m.segment0_windows=1;
 m.windows[0]=(struct pci813_window){
  UINT64_C(0xe0000000),UINT64_C(0xe0000000),UINT64_C(0xf0000000),0,0,255};
 prepare(&root);assert(pci814_correlate(&root,1,&m,&topo,&report));
 assert(report.roots==1&&report.segment0_roots==1&&report.covered_buses==3);
 assert(!pci814_allow_dma(0,0));
 unchanged=root;
 buffer[1]=42;assert(!pci814_parse_configuration(0,buffer,48,&root));
 assert(!memcmp(&unchanged,&root,sizeof root));
 prepare(&root);buffer[39]=0;assert(!pci814_parse_configuration(0,buffer,48,&root));
 prepare(&root);buffer[30]=1;assert(!pci814_parse_configuration(0,buffer,48,&root));
 prepare(&root);buffer[3]=9;assert(!pci814_parse_configuration(0,buffer,48,&root));
 prepare(&root);buffer[46]=0;assert(!pci814_parse_configuration(0,buffer,48,&root));
 prepare(&root);buffer[22]=0;assert(!pci814_parse_configuration(0,buffer,48,&root));
 prepare(&root);root.bus_present[5]=0;oldreport=report;
 assert(!pci814_correlate(&root,1,&m,&topo,&report));
 assert(!memcmp(&oldreport,&report,sizeof report));
 prepare(&root);m.windows[0].last_bus=1;
 assert(!pci814_correlate(&root,1,&m,&topo,&report));
 m.windows[0].last_bus=255;root.segment=3;
 assert(!pci814_correlate(&root,1,&m,&topo,&report));
 prepare(&root);
 struct pci814_root twice[2]={root,root};
 assert(!pci814_correlate(twice,2,&m,&topo,&report));
 twice[1].segment=2;
 assert(pci814_correlate(twice,2,&m,&topo,&report)&&report.other_segment_roots==1);
 assert(!pci814_correlate(0,1,&m,&topo,&report));
 assert(!pci814_parse_configuration(0,0,0,&root));
 puts("STYLEOS S814 HOST PASS bounded EFI root-bridge resources, PCI buses and DMA deny");
 return 0;
}
