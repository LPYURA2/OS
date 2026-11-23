#include <styleos/pci_requester_811.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void w16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void w32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void w64(uint8_t*p,uint64_t v){w32(p,(uint32_t)v);w32(p+4,(uint32_t)(v>>32));}
static void fix(uint8_t*p,size_t n){p[9]=0;uint8_t s=0;for(size_t i=0;i<n;i++)s=(uint8_t)(s+p[i]);p[9]=(uint8_t)(0u-s);}
static void intel(uint8_t*p){
 memset(p,0,128);memcpy(p,"DMAR",4);w32(p+4,80);p[8]=1;p[36]=39;
 w16(p+48,0);w16(p+50,32);p[52]=0;w64(p+56,0xfed90000);
 p[64]=1;p[65]=8;p[69]=0;p[70]=2;p[71]=1; /* BDF 00:02.1 */
 p[72]=2;p[73]=8;p[77]=0;p[78]=3;p[79]=0; /* PCI bridge scope: unresolved */
 fix(p,80);
}
static void amd(uint8_t*p){
 memset(p,0,128);memcpy(p,"IVRS",4);w32(p+4,92);p[8]=2;
 p[48]=0x10;w16(p+50,44);w16(p+52,2);w64(p+56,0xfed80000);
 p[72]=2;w16(p+73,0x11);p[75]=0;
 p[76]=3;w16(p+77,0x20);p[79]=0;
 p[80]=4;w16(p+81,0x22);p[83]=0;
 p[84]=1;w16(p+85,0);p[87]=0;
 p[88]=0; /* unsupported 4-byte (must be skipped, not authorized) */
 fix(p,92);
}
int main(void){
 uint8_t p[128]={0};struct pci811_map m={0};uint8_t u=99;
 intel(p);assert(pci811_dmar_decode(p,80,&m));
 assert(m.units==1&&m.exact_count==1&&m.unresolved==1&&m.include_all==0);
 assert(pci811_lookup(&m,0x11,&u)==1&&u==0);
 assert(pci811_lookup(&m,0x18,&u)==0);assert(!pci811_allow_dma(&m,0x11));
 p[9]^=1;assert(!pci811_dmar_decode(p,80,&m));p[9]^=1;
 p[70]=32;fix(p,80);assert(pci811_dmar_decode(p,80,&m)&&m.exact_count==0&&m.unresolved==2);
 intel(p);p[48]=1;fix(p,80);assert(!pci811_dmar_decode(p,80,&m));
 amd(p);assert(pci811_ivrs_decode(p,92,&m));
 assert(m.units==1&&m.exact_count==4&&m.ranges==1&&m.include_all==1&&m.unresolved==1);
 assert(pci811_lookup(&m,0x11,&u)==1&&u==0);assert(pci811_lookup(&m,0x21,&u)==1);
 assert(pci811_lookup(&m,0x24,&u)==0);assert(!pci811_allow_dma(&m,0x21));
 p[81]=0x1f;fix(p,92);assert(!pci811_ivrs_decode(p,92,&m));
 amd(p);p[88]=8;fix(p,92);assert(pci811_ivrs_decode(p,92,&m)&&m.unresolved>=1);
 amd(p);p[84]=4;fix(p,92);assert(!pci811_ivrs_decode(p,92,&m));
 amd(p);p[9]^=1;assert(!pci811_ivrs_decode(p,92,&m));
 assert(!pci811_dmar_decode(0,0,&m));assert(!pci811_ivrs_decode(0,0,&m));
 assert(pci811_lookup(0,1,&u)==-1);assert(pci811_lookup(&m,1,0)==-1);
 puts("STYLEOS S811 HOST PASS explicit requester scopes range fallback and fail-closed");return 0;
}
