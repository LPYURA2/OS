#include <styleos/pci_requester_812.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static struct pci812_topology topo;
static void put16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t*p,uint32_t v){for(unsigned j=0;j<4;j++)p[j]=(uint8_t)(v>>(8u*j));}
static void put64(uint8_t*p,uint64_t v){put32(p,(uint32_t)v);put32(p+4,(uint32_t)(v>>32));}
static void checksum(uint8_t*p,size_t len){p[9]=0;unsigned sum=0;for(size_t j=0;j<len;j++)sum+=p[j];p[9]=(uint8_t)(0u-sum);}
static void dmar(uint8_t*p){
 memset(p,0,160);memcpy(p,"DMAR",4);put32(p+4,86);p[8]=1;p[36]=39;
 put16(p+48,0);put16(p+50,38);put64(p+56,0xfed90000u);
 p[64]=1;p[65]=12;p[69]=0;
 p[70]=1;p[71]=0;p[72]=3;p[73]=0;p[74]=2;p[75]=1;
 p[76]=2;p[77]=10;p[81]=0;p[82]=1;p[83]=0;p[84]=3;p[85]=0;
 checksum(p,86);
}
static void ivrs(uint8_t*p){
 memset(p,0,192);memcpy(p,"IVRS",4);put32(p+4,144);p[8]=2;
 p[48]=0x10;put16(p+50,64);put16(p+52,2);put64(p+56,0xfed80000u);
 p[72]=2;put16(p+73,0x511);
 p[76]=0x42;put16(p+77,0x600);put16(p+81,0x511);
 p[84]=0x46;put16(p+85,0x700);put32(p+88,0x11223344u);
 p[92]=3;put16(p+93,0x40);
 p[96]=4;put16(p+97,0x42);
 p[100]=0x48;put16(p+101,0x800);put32(p+104,0x00051107u);
 p[108]=1;
 p[112]=0x20;p[113]=0x08;put16(p+114,32);
 put64(p+128,0x100000u);put64(p+136,0x1000u);
 checksum(p,144);
}
int main(void){
 static uint8_t p[192];struct pci812_audit a={0};uint8_t unit=99;
 pci812_topology_init(&topo);
 assert(topo.buses==1&&topo.functions==0);
 assert(pci812_add_function(&topo,0,1,0,2,2,7));
 assert(pci812_add_function(&topo,2,3,0,2,5,7));
 assert(pci812_add_function(&topo,5,2,1,1,0,0));
 const uint8_t path[]={1,0,3,0,2,1};uint16_t rid=0;
 assert(pci812_resolve(&topo,0,path,3,1,&rid)&&rid==0x511);
 assert(!pci812_resolve(&topo,0,path,3,2,&rid));
 assert(!pci812_add_function(&topo,5,2,1,1,0,0));
 assert(!pci812_add_function(&topo,0,4,0,2,2,7)); /* duplicate downstream */
 assert(!pci812_add_function(&topo,0,5,0,2,8,3)); /* inverted range */
 assert(!pci812_add_function(&topo,3,0,0,1,0,0)); /* not discovered */
 assert(topo.buses==3&&topo.bridges==2&&topo.functions==3);
 dmar(p);assert(pci812_dmar_decode(p,86,&topo,&a));
 assert(a.units==1&&a.exact==1&&a.bridge_scopes==1&&a.unresolved==0);
 assert(pci812_lookup(&a,0x511,&unit)==1&&unit==0);
 assert(pci812_lookup(&a,0x512,&unit)==0);
 assert(!pci812_allow_dma(&a,0x511));
 p[9]^=1;assert(!pci812_dmar_decode(p,86,&topo,&a));p[9]^=1;
 p[74]=7;checksum(p,86);
 assert(pci812_dmar_decode(p,86,&topo,&a)&&a.unresolved==1&&a.exact==0);
 dmar(p);p[81]=1;checksum(p,86);
 assert(pci812_dmar_decode(p,86,&topo,&a)&&a.bridge_scopes==0&&a.unresolved==1);
 ivrs(p);assert(pci812_ivrs_decode(p,144,&topo,&a));
 assert(a.units==1&&a.exact==5&&a.aliases==1&&a.extended==1);
 assert(a.ranges==1&&a.special==1&&a.include_all==1&&a.exceptions==1);
 assert(a.alias[0].from==0x600&&a.alias[0].to==0x511);
 assert(pci812_lookup(&a,0x600,&unit)==0); /* alias is never exact owner */
 assert(pci812_lookup(&a,0x700,&unit)==1);
 assert(!pci812_allow_dma(&a,0x600));
 p[97]=0x30;checksum(p,144);
 assert(!pci812_ivrs_decode(p,144,&topo,&a)); /* end before start */
 ivrs(p);p[77]=0x11;p[78]=0x05;checksum(p,144);
 assert(!pci812_ivrs_decode(p,144,&topo,&a)); /* alias source collides with select */
 ivrs(p);p[9]^=1;assert(!pci812_ivrs_decode(p,144,&topo,&a));
 assert(!pci812_ivrs_decode(0,0,&topo,&a));
 assert(!pci812_dmar_decode(0,0,&topo,&a));
 assert(pci812_lookup(0,0,&unit)==-1&&pci812_lookup(&a,0,0)==-1);
 puts("STYLEOS S812 HOST PASS multihop DMAR bridge scope IVHD alias/extended/exclusions fail-closed");
 return 0;
}
