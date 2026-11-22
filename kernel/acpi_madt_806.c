#include <styleos/acpi_madt_806.h>
#include <stdint.h>
#include <stddef.h>
static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t le64(const uint8_t *p){return (uint64_t)le32(p)|((uint64_t)le32(p+4)<<32);}
static int sig(const uint8_t *p,const char *s,size_t n){for(size_t i=0;i<n;i++)if(p[i]!=(uint8_t)s[i])return 0;return 1;}
static int csum(const uint8_t *p,size_t n){unsigned x=0;for(size_t i=0;i<n;i++)x=(x+p[i])&255u;return x==0;}
static int sdt(const uint8_t *p,size_t n,const char *name,size_t max){
 if(!p||n<36||n>max||!sig(p,name,4))return 0;
 uint32_t len=le32(p+4);return len==n && len>=36 && len<=max && csum(p,n);
}
int acpi806_rsdp(const uint8_t *p,size_t n,uint64_t *xsdt){
 if(!p||!xsdt||n<36||!sig(p,"RSD PTR ",8)||!csum(p,20))return 0;
 if(p[15]<2||le32(p+20)!=36||!csum(p,36))return 0;
 uint64_t address=le64(p+24);
 if(!address||(address&7u)||address>UINT64_C(0xfffffffff8))return 0;
 *xsdt=address;return 1;
}
int acpi806_xsdt(const uint8_t *p,size_t n,uint64_t *entries,size_t *count){
 if(!entries||!count||!sdt(p,n,"XSDT",36+ACPI806_MAX_ROOT*8))return 0;
 if((n-36)%8u||n==36)return 0;
 size_t k=(n-36)/8u;
 for(size_t i=0;i<k;i++){
  uint64_t a=le64(p+36+i*8u);
  if(!a||(a&3u)||a>UINT64_C(0xfffffffffc))return 0;
  for(size_t j=0;j<i;j++)if(entries[j]==a)return 0;
  entries[i]=a;
 }
 *count=k;return 1;
}
int acpi806_madt(const uint8_t *p,size_t n,struct acpi806_topology *out){
 if(!out||!sdt(p,n,"APIC",4096)||n<44)return 0;
 struct acpi806_topology t={0};
 t.madt_length=(uint32_t)n;t.lapic_physical=le32(p+36);t.madt_flags=le32(p+40);
 if(!t.lapic_physical || (t.lapic_physical&0xfffu))return 0;
 size_t i=44;
 while(i<n){
  if(n-i<2)return 0;
  unsigned type=p[i],len=p[i+1];
  if(len<2||len>n-i)return 0;
  const uint8_t *e=p+i;
  if(type==0){
   if(len!=8)return 0;
   if((le32(e+4)&1u)!=0){if(++t.enabled_lapics>256)return 0;}
  }else if(type==1){
   if(len!=12||t.ioapic_count>=ACPI806_MAX_IOAPIC)return 0;
   struct acpi806_ioapic v={(uint32_t)e[2],le32(e+4),le32(e+8)};
   if(!v.physical || (v.physical&4095u))return 0;
   for(uint32_t j=0;j<t.ioapic_count;j++){
    if(t.ioapic[j].id==v.id ||t.ioapic[j].gsi_base==v.gsi_base ||t.ioapic[j].physical==v.physical)return 0;
   }
   t.ioapic[t.ioapic_count++]=v;
  }else if(type==2){
   if(len!=10||t.iso_count>=ACPI806_MAX_ISO)return 0;
   struct acpi806_override v={(uint32_t)e[2],(uint32_t)e[3],le32(e+4),le16(e+8)};
   if(v.bus!=0||v.source>=16|| (v.flags&0xfff0u) || (v.flags&3u)==2u || ((v.flags>>2)&3u)==2u)return 0;
   for(uint32_t j=0;j<t.iso_count;j++)if(t.iso[j].source==v.source)return 0;
   t.iso[t.iso_count++]=v;
  }else if(type==5){
   if(len!=12)return 0;
   uint64_t addr=le64(e+4);
   if(!addr || addr>UINT32_MAX || (addr&4095u))return 0;
   t.lapic_physical=(uint32_t)addr;
  }else if(type==9){
   if(len!=16)return 0;
   if(le32(e+8)&1u){if(++t.enabled_lapics>256)return 0;}
  }
  i+=len;
 }
 if(t.ioapic_count==0||t.enabled_lapics==0)return 0;
 *out=t;return 1;
}
