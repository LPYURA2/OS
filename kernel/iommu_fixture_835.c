#include <styleos/iommu_fixture_835.h>
#include <styleos/dmar_808.h>
#include <styleos/iommu_platform_810.h>
#include <styleos/acpi_madt_806.h>
static uint64_t rd(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static int add(struct s835_input *x,uint64_t a,uint64_t b){
 if(a>=b||x->nr_others==S835_OTHER_MAX)return 0;
 x->others[x->nr_others++]=(struct boot_range){a,b};return 1;
}
static int resources(struct s835_input *x,const uint8_t *p,size_t n,int uefi){
 if(!p||n<2||n>(uefi?1200u:1024u))return 0;
 unsigned count=0;
 for(size_t i=0;i<n;){
  unsigned tag=p[i];if(tag==0x79){unsigned sum=0;for(size_t j=0;j<n;j++)sum+=p[j];return i+2==n&&(!p[i+1]||!(sum&255u));}
  if(++count>(uefi?24u:64u))return 0;
  if(!uefi&&tag==0x47){if(n-i<8||p[i+1]!=1||rd(p+i+2,2)!=0xcf8||rd(p+i+4,2)!=0xcf8||p[i+7]!=8)return 0;i+=8;continue;}
  unsigned w=tag==0x88?2:tag==0x87?4:tag==0x8a?8:0;
  if(!w||(uefi&&w!=8)||n-i<6u+5u*w||rd(p+i+1,2)!=3u+5u*w)return 0;
  const uint8_t *q=p+i+3;unsigned kind=q[0];uint64_t gran=rd(q+3,w),lo=rd(q+3+w,w),hi=rd(q+3+2*w,w),tr=rd(q+3+3*w,w),len=rd(q+3+4*w,w);
  if(kind>2||tr||lo>hi||hi==UINT64_MAX||!len||hi-lo+1!=len||q[1]!=(uefi?0:12))return 0;
  if(kind==0){if(uefi?(q[2]!=0||(gran!=32&&gran!=64)||(gran==32&&hi>UINT32_MAX)):(gran!=0||(q[2]!=1&&q[2]!=3)))return 0;if(!add(x,lo,hi+1))return 0;}
  i+=6u+5u*w;
 }
 return 0;
}
int s835_input_from_tables(unsigned backend,const uint8_t *table,size_t tn,const uint8_t *madt,size_t an,const uint8_t *crs,size_t cn,const uint8_t *root,size_t rn,struct s835_input *out){
 if(!out||backend>2)return 0;
 struct s835_input x={0};x.backend=backend;
 if(backend==0){if(tn)return 0;}
 else if(backend==1){struct dmar808_info d={0};if(!dmar808_parse(table,tn,&d)||d.drhd_count!=1||d.drhd[0].segment||d.drhd[0].base!=UINT64_C(0xfed90000))return 0;x.aperture=(struct boot_range){0xfed90000,0xfed90230};}
 else {struct iommu810_ivrs d={0};if(!ivrs810_parse(table,tn,&d)||d.ivhd_count!=1||d.units[0].pci_segment||d.units[0].mmio_base!=UINT64_C(0xfed80000))return 0;x.aperture=(struct boot_range){0xfed80000,0xfed84000};}
 struct acpi806_topology m={0};if(!acpi806_madt(madt,an,&m)||!m.ioapic_count)return 0;
 if(!resources(&x,crs,cn,0)||!resources(&x,root,rn,1))return 0;
 for(unsigned i=0;i<m.ioapic_count;i++){uint64_t a=m.ioapic[i].physical;if(!a||(a&4095u)||!add(&x,a,a+4096))return 0;}
 *out=x;return 1;
}
