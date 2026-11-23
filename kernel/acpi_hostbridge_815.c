#include <styleos/acpi_hostbridge_815.h>
#include <stddef.h>
#include <stdint.h>
static uint16_t rd16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t rd64(const uint8_t *p){return (uint64_t)rd32(p)|((uint64_t)rd32(p+4)<<32);}
static int sum0(const uint8_t *p,size_t n){uint8_t sum=0;for(size_t i=0;i<n;i++)sum=(uint8_t)(sum+p[i]);return sum==0;}
static int sig(const uint8_t *p,const char *s){return p[0]==(uint8_t)s[0]&&p[1]==(uint8_t)s[1]&&p[2]==(uint8_t)s[2]&&p[3]==(uint8_t)s[3];}
int acpi815_fadt_dsdt(const uint8_t *p,size_t n,uint64_t *address){
 if(!p||!address||n<44u||n>4096u||!sig(p,"FACP")||rd32(p+4)!=n||!sum0(p,n))return 0;
 uint64_t old=rd32(p+40),modern=n>=148u?rd64(p+140):0;
 if(!old&&!modern)return 0;
 if(modern&&old&&modern<=UINT32_MAX&&(uint32_t)modern!=(uint32_t)old)return 0;
 uint64_t result=modern?modern:old;
 if(!result||(result&3u)||result>UINT64_MAX-ACPI815_MAX_DSDT)return 0;
 *address=result;return 1;
}
static int pkg(const uint8_t *p,size_t avail,size_t *body,size_t *total){
 if(!p||!body||!total||!avail)return 0;
 unsigned extra=p[0]>>6u;
 if(avail<1u+extra)return 0;
 size_t length=extra?(size_t)(p[0]&0x0fu):(size_t)(p[0]&0x3fu);
 for(unsigned j=0;j<extra;j++)length|=(size_t)p[1u+j]<<(4u+8u*j);
 if(length<1u+extra||length>avail)return 0;
 *body=1u+extra;*total=length;return 1;
}
static int name(const uint8_t *p,size_t n,size_t *taken){
 if(!p||!taken||!n)return 0;
 size_t i=0;
 if(p[i]=='\\')i++;
 while(i<n&&p[i]=='^')i++;
 if(i>=n)return 0;
 unsigned components=1;
 if(p[i]==0x2e){components=2;i++;}
 else if(p[i]==0x2f){if(i+2u>n)return 0;components=p[i+1];i+=2;}
 else if(p[i]==0x00){*taken=i+1;return 1;}
 if(components==0||components>16||i+(size_t)components*4u>n)return 0;
 for(unsigned j=0;j<components*4u;j++){
  uint8_t c=p[i+j];if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return 0;
 }
 *taken=i+(size_t)components*4u;return 1;
}
static int uint_const(const uint8_t *p,size_t n,uint64_t *value){
 if(!n||!value)return 0;
 if(p[0]==0||p[0]==1){*value=p[0];return 1;}
 if(p[0]==0xff){*value=UINT64_MAX;return 1;}
 if(p[0]==0x0a&&n>=2){*value=p[1];return 1;}
 if(p[0]==0x0b&&n>=3){*value=rd16(p+1);return 1;}
 if(p[0]==0x0c&&n>=5){*value=rd32(p+1);return 1;}
 if(p[0]==0x0e&&n>=9){*value=rd64(p+1);return 1;}
 return 0;
}
/* Static Memory32Fixed/QWord candidate, never an ACPI evaluated value. */
static unsigned resource_covers(const uint8_t *p,size_t n,const struct pci813_mcfg *m){
 unsigned matches=0;
 for(size_t off=0;off<n;){
  unsigned type=p[off];size_t size;
  if(type&0x80u){if(n-off<3)return 0;size=3u+rd16(p+off+1u);}
  else size=1u+(type&7u);
  if(size>n-off)return 0;
  uint64_t start=0,end=0;
  if(type==0x86&&size==12){
   start=rd32(p+off+4);uint64_t length=rd32(p+off+8);
   if(length&&start<=UINT64_MAX-length)end=start+length;
  }else if(type==0x8a&&size==46&&p[off+3]==0){
   start=rd64(p+off+14);uint64_t length=rd64(p+off+38);
   if(length&&start<=UINT64_MAX-length)end=start+length;
  }
  if(end>start)for(unsigned i=0;i<m->count;i++)
   if(m->windows[i].segment==0&&start<=m->windows[i].physical_first&&
      end>=m->windows[i].physical_end){matches++;break;}
  if(type==0x79)break;
  off+=size;
 }
 return matches;
}
static void inspect_device(const uint8_t *p,size_t n,const struct pci813_mcfg *m,struct acpi815_report *r){
 size_t scan=n<192?n:192;int hid=0;
 for(size_t i=0;i+10u<=scan;i++)if(p[i]==8&&sig(p+i+1,"_HID")){
  const uint8_t *v=p+i+5;
  if(v[0]==0x0c&&v[1]==0x41&&v[2]==0xd0){
   if(v[3]==0x0a&&(v[4]==0x03||v[4]==0x08))hid=1;
   else if(v[3]==0x0c&&v[4]==0x02)hid=2;
  }
  if(v[0]==0x0d&&i+5u+9u<=scan&&v[1]=='P'&&v[2]=='N'&&v[3]=='P'&&v[4]=='0'){
   if(v[5]=='A'&&v[6]=='0'&&(v[7]=='3'||v[7]=='8')&&v[8]==0)hid=1;
   if(v[5]=='C'&&v[6]=='0'&&v[7]=='2'&&v[8]==0)hid=2;
  }
  if(hid)break;
 }
 if(!hid)return;
 if(hid==1)r->pci_hid_candidates++;else r->motherboard_hid_candidates++;
 for(size_t i=0;i+6u<=scan;i++){
  if(p[i]==0x08&&sig(p+i+1,"_SEG")&&hid==1){
   uint64_t v;if(uint_const(p+i+5,scan-i-5,&v)){
    r->seg_static_candidates++;if(v==0)r->segment0_static_observed=1;
   }else r->unsupported_names++;
  }else if(p[i]==0x08&&sig(p+i+1,"_BBN")&&hid==1){
   uint64_t v;if(uint_const(p+i+5,scan-i-5,&v)){
    r->bbn_static_candidates++;if(v==0)r->bus0_static_observed=1;
   }else r->unsupported_names++;
  }else if(p[i]==0x14){
   size_t body,total;
   if(pkg(p+i+1,scan-i-1,&body,&total)&&total>=body+5u&&
      sig(p+i+1+body,"_CRS"))r->crs_method_candidates++;
  }else if(p[i]==0x08&&sig(p+i+1,"_CRS")){
   if(p[i+5]!=0x11){r->unsupported_names++;continue;}
   size_t body,total;const uint8_t *q=p+i+6;
   if(!pkg(q,n-i-6,&body,&total)||body>=total){r->malformed_candidates++;continue;}
   uint64_t claimed=0;const uint8_t *sz=q+body;
   if(!uint_const(sz,total-body,&claimed)||claimed>ACPI815_MAX_DSDT){r->unsupported_names++;continue;}
   size_t integer_len=(sz[0]==0x0a?2u:sz[0]==0x0b?3u:sz[0]==0x0c?5u:sz[0]==0x0e?9u:1u);
   if(total<body+integer_len||claimed>total-body-integer_len){r->malformed_candidates++;continue;}
   r->crs_static_candidates++;
   if(hid==2){
    unsigned matching=resource_covers(sz+integer_len,(size_t)claimed,m);
    if(matching){r->ecam_literal_candidates++;r->ecam_literal_matches+=(uint16_t)matching;}
   }
  }
 }
}
int acpi815_inspect(const uint8_t *p,size_t n,const struct pci813_mcfg *m,
 unsigned ssdt_count,struct acpi815_report *out){
 if(!p||!out||!m||!m->count||n<37u||n>ACPI815_MAX_DSDT||
    !sig(p,"DSDT")||rd32(p+4)!=n||!sum0(p,n)||ssdt_count>ACPI815_MAX_SSDT)return 0;
 struct acpi815_report r={0};r.dsdt_length=(uint32_t)n;r.ssdt_count=(uint16_t)ssdt_count;
 for(size_t i=36;i+8u<n;i++)if(p[i]==0x5b&&p[i+1]==0x82){
  size_t body,total,ns=0;
  if(!pkg(p+i+2,n-(i+2),&body,&total))continue;
  if(!name(p+i+2+body,total-body,&ns))continue;
  size_t start=i+2+body+ns,end=i+2+total;
  if(start>=end)continue;
  r.aml_device_packages++;inspect_device(p+start,end-start,m,&r);
  if(r.aml_device_packages>=512u)break;
 }
 r.acpi_namespace_evaluated=0;r.crs_evaluated=0;r.iommu_owned=0;
 *out=r;return 1;
}
int acpi815_allow_dma(const struct acpi815_report *r,unsigned rid){(void)r;(void)rid;return 0;}
