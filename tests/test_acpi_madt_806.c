#include <styleos/acpi_madt_806.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
static void set16(uint8_t *p,uint16_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void set32(uint8_t *p,uint32_t x){for(int i=0;i<4;i++)p[i]=(uint8_t)(x>>(i*8));}
static void set64(uint8_t *p,uint64_t x){set32(p,(uint32_t)x);set32(p+4,(uint32_t)(x>>32));}
static void checksum(uint8_t *p,size_t n,size_t slot){p[slot]=0;unsigned sum=0;for(size_t i=0;i<n;i++)sum+=p[i];p[slot]=(uint8_t)(0u-sum);}
int main(void){
 uint8_t rsdp[36]={0},xsdt[52]={0},madt[74]={0};
 memcpy(rsdp,"RSD PTR ",8);rsdp[15]=2;set32(rsdp+20,36);set64(rsdp+24,0x12345000);
 checksum(rsdp,20,8);checksum(rsdp,36,32);
 uint64_t addr=0;assert(acpi806_rsdp(rsdp,36,&addr)&&addr==0x12345000);
 memcpy(xsdt,"XSDT",4);set32(xsdt+4,sizeof xsdt);set64(xsdt+36,0x2000);set64(xsdt+44,0x3000);checksum(xsdt,sizeof xsdt,9);
 uint64_t entries[ACPI806_MAX_ROOT];size_t count=0;assert(acpi806_xsdt(xsdt,sizeof xsdt,entries,&count)&&count==2);
 memcpy(madt,"APIC",4);set32(madt+4,sizeof madt);set32(madt+36,0xfee00000);set32(madt+40,1);
 madt[44]=0;madt[45]=8;madt[46]=0;madt[47]=0;set32(madt+48,1);
 madt[52]=1;madt[53]=12;madt[54]=2;madt[55]=0;set32(madt+56,0xfec00000);set32(madt+60,0);
 madt[64]=2;madt[65]=10;madt[66]=0;madt[67]=0;set32(madt+68,2);set16(madt+72,0);
 checksum(madt,sizeof madt,9);
 struct acpi806_topology t={0};assert(acpi806_madt(madt,sizeof madt,&t));
 assert(t.ioapic_count==1 && t.ioapic[0].physical==0xfec00000 &&t.iso_count==1&&t.iso[0].gsi==2&&t.enabled_lapics==1);
 unsigned cases=0;
 #define BAD(expr) do{assert(!(expr));cases++;}while(0)
 BAD(acpi806_rsdp(rsdp,20,&addr));rsdp[10]^=1;BAD(acpi806_rsdp(rsdp,36,&addr));rsdp[10]^=1;
 rsdp[32]^=1;BAD(acpi806_rsdp(rsdp,36,&addr));rsdp[32]^=1;
 set64(rsdp+24,0);checksum(rsdp,36,32);BAD(acpi806_rsdp(rsdp,36,&addr));
 set64(rsdp+24,0x12345000);checksum(rsdp,36,32);
 xsdt[9]^=1;BAD(acpi806_xsdt(xsdt,sizeof xsdt,entries,&count));xsdt[9]^=1;
 set64(xsdt+44,0x2000);checksum(xsdt,sizeof xsdt,9);BAD(acpi806_xsdt(xsdt,sizeof xsdt,entries,&count));
 set64(xsdt+44,0x3000);checksum(xsdt,sizeof xsdt,9);
 madt[9]^=1;BAD(acpi806_madt(madt,sizeof madt,&t));madt[9]^=1;
 madt[65]=0;checksum(madt,sizeof madt,9);BAD(acpi806_madt(madt,sizeof madt,&t));madt[65]=10;checksum(madt,sizeof madt,9);
 madt[45]=7;checksum(madt,sizeof madt,9);BAD(acpi806_madt(madt,sizeof madt,&t));madt[45]=8;checksum(madt,sizeof madt,9);
 madt[67]=16;checksum(madt,sizeof madt,9);BAD(acpi806_madt(madt,sizeof madt,&t));madt[67]=0;checksum(madt,sizeof madt,9);
 madt[73]=2;checksum(madt,sizeof madt,9);BAD(acpi806_madt(madt,sizeof madt,&t));madt[73]=0;checksum(madt,sizeof madt,9);
 set32(madt+56,0);checksum(madt,sizeof madt,9);BAD(acpi806_madt(madt,sizeof madt,&t));
 printf("STYLEOS S806 host ACPI checksum/length/route negative PASS %u\n",cases);
 return 0;
}
