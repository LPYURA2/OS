#include <styleos/acpi_namespace_816.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t dsdt[128],ssdt[128];
static void w32(uint8_t*p,uint32_t v){for(unsigned j=0;j<4;j++)p[j]=(uint8_t)(v>>(8*j));}
static void finish(uint8_t *p,size_t len,const char *sig){
 memcpy(p,sig,4);w32(p+4,(uint32_t)len);p[8]=2;p[9]=0;
 unsigned sum=0;for(size_t i=0;i<len;i++)sum+=p[i];p[9]=(uint8_t)(0u-sum);
}
static size_t make_dsdt(void){
 memset(dsdt,0,sizeof dsdt);size_t i=36;
 dsdt[i++]=0x5b;dsdt[i++]=0x82;size_t device_pkg=i++;
 memcpy(dsdt+i,"PCI0",4);i+=4;
 const uint8_t hid[]={0x08,'_','H','I','D',0x0c,0x41,0xd0,0x0a,0x08};
 memcpy(dsdt+i,hid,sizeof hid);i+=sizeof hid;
 const uint8_t seg[]={0x08,'_','S','E','G',0x00};
 memcpy(dsdt+i,seg,sizeof seg);i+=sizeof seg;
 const uint8_t bbn[]={0x14,0x08,'_','B','B','N',0x00,0xa4,0x00};
 memcpy(dsdt+i,bbn,sizeof bbn);i+=sizeof bbn;
 dsdt[device_pkg]=(uint8_t)(i-device_pkg);
 finish(dsdt,i,"DSDT");return i;
}
static size_t make_ssdt(void){
 memset(ssdt,0,sizeof ssdt);size_t i=36;
 ssdt[i++]=0x10;size_t scope_pkg=i++;
 memcpy(ssdt+i,"PCI0",4);i+=4;
 const uint8_t crs[]={0x14,0x0d,'_','C','R','S',0x00,0xa4,
                      0x11,0x05,0x0a,0x02,0x79,0x00};
 memcpy(ssdt+i,crs,sizeof crs);i+=sizeof crs;
 ssdt[scope_pkg]=(uint8_t)(i-scope_pkg);
 finish(ssdt,i,"SSDT");return i;
}
int main(void){
 size_t n=make_dsdt(),s=make_ssdt();struct acpi816_table tbl[2]={{dsdt,n},{ssdt,s}};
 static struct acpi816_result r;
 assert(acpi816_inspect(tbl,2,&r));
 assert(r.tables==2&&r.ssdts==1&&r.devices==1&&r.scopes==1);
 assert(r.complete&&r.hostbridge_literal_ready&&r.seg_known&&r.bbn_known&&r.crs_known);
 assert(r.hostbridge_segment==0&&r.hostbridge_bus==0&&r.crs_length==2);
 assert(r.objects==4&&!r.regions_denied&&!r.duplicates);
 for(unsigned id=0;id<65536;id+=257)assert(!acpi816_allow_dma(&r,(uint16_t)id));
 struct acpi816_result before=r;
 ssdt[9]^=1;assert(!acpi816_inspect(tbl,2,&r));assert(!memcmp(&before,&r,sizeof r));
 ssdt[9]^=1;
 assert(!acpi816_inspect(tbl,0,&r));assert(!acpi816_inspect(0,2,&r));
 assert(!acpi816_inspect(tbl,10,&r));
 /* Duplicate object in SSDT must never become a second permission. */
 s=make_ssdt();ssdt[42]='_';ssdt[43]='S';ssdt[44]='E';ssdt[45]='G';finish(ssdt,s,"SSDT");
 assert(acpi816_inspect(tbl,2,&r)&&!r.complete&&!r.hostbridge_literal_ready);
 /* Recovered source even after an unsupported byte cannot fake NameOp. */
 s=make_ssdt();ssdt[36]=0x5b;finish(ssdt,s,"SSDT");
 assert(acpi816_inspect(tbl,2,&r)&&!r.complete&&!r.hostbridge_literal_ready);
 /* OperationRegion is never performed and poisons completeness. */
 s=make_ssdt();ssdt[42]=0x5b;ssdt[43]=0x80;finish(ssdt,s,"SSDT");
 assert(acpi816_inspect(tbl,2,&r)&&!r.complete&&!r.hostbridge_literal_ready);
 /* Method with a nonliteral return does not get 'executed'. */
 s=make_ssdt();ssdt[50]=0x70;finish(ssdt,s,"SSDT");
 assert(acpi816_inspect(tbl,2,&r)&&!r.complete&&!r.hostbridge_literal_ready);
 /* Fully valid unrecognised firmware is allowed as incomplete inventory. */
 n=make_dsdt();dsdt[36]=0x80;finish(dsdt,n,"DSDT");
 assert(acpi816_inspect(tbl,1,&r)&&!r.complete&&!r.hostbridge_literal_ready);
 puts("STYLEOS S816 HOST PASS literal-only DSDT SSDT namespace, methods, OperationRegion refusal, DMA deny");
 return 0;
}
