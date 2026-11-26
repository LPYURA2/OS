/* Acceptance-only external launch provider; never compiled into ordinary image. */
#include <styleos/native_runtime_865.h>
#include <styleos/arch.h>
#include "../../boot/x86_64/native_executor_865.h"
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
#error catalog driver requires explicit acceptance profile
#endif
static void native_wipe(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static void field(const char *key,uint64_t v){arch_putc(' ');arch_puts(key);arch_puts("=0x");for(int j=60;j>=0;j-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)j)&15]);}
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
#include "native_catalog_digest.h"
/* Explicitly provisioned allowlist. Digest is trusted build input, not read
 * from the medium. No executable bytes or test scheduling policy linked.
 * All record bounds are checked before admitting even the first app. */
#define NC_FIRST 3000u
#define NC_SECTORS 4u
#define NC_RECORD 48u
static uint8_t catalog[NC_SECTORS*512];
static bool native_catalog_busy;
bool s863_disk_read(uint32_t,uint8_t[512]);
static bool catalog_read(void *unused,uint32_t lba,uint8_t out[512]){
 (void)unused;return lba>=3072u&&lba<4095u&&s863_disk_read(lba,out);
}
static uint32_t nc_u32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool nc_equal(const uint8_t *a,const uint8_t *b,size_t n){unsigned diff=0;for(size_t j=0;j<n;j++)diff|=a[j]^b[j];return !diff;}
static bool catalog_valid(unsigned *out){
 for(unsigned j=0;j<NC_SECTORS;j++)if(!s863_disk_read(NC_FIRST+j,catalog+512*j))return false;
 uint8_t digest[32];nte_sha256(catalog,sizeof catalog,digest);
 static const uint8_t magic[8]={'S','T','Y','L','N','A','T','1'};
 if(!nc_equal(digest,native_catalog_digest,32)||!nc_equal(catalog,magic,8)||nc_u32(catalog+8)!=1)return false;
 unsigned count=nc_u32(catalog+12);
 if(!count||count>S846_ROOT_COUNT||16+count*NC_RECORD>sizeof catalog)return false;
 for(unsigned j=0;j<count;j++){
  const uint8_t *r=catalog+16+j*NC_RECORD;
  uint32_t lba=nc_u32(r),bytes=nc_u32(r+4);
  if(lba<3072u||lba>=4095u||bytes<120u||bytes>8192u||(bytes+511)/512>4095u-lba)return false;
  bool named=false,ended=false,hashed=false;
  for(unsigned k=0;k<32;k++)if(r[8+k])hashed=true;
  for(unsigned k=0;k<8;k++){
   unsigned ch=r[40+k];if(!ch){ended=true;continue;}
   if(ended||!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_'))return false;
   named=true;
  }
  if(!named||!hashed)return false;
  for(unsigned k=0;k<j;k++){
   const uint8_t *q=catalog+16+k*NC_RECORD;
   uint32_t first=nc_u32(q),last=first+(nc_u32(q+4)+511)/512;
   if(lba<last&&first<lba+(bytes+511)/512)return false;
  }
 }
 for(size_t j=16+count*NC_RECORD;j<sizeof catalog;j++)if(catalog[j])return false;
 *out=count;return true;
}
bool s865_native_catalog_request(void){
 if(native_catalog_busy||!s865_executor_idle()||
    pr_snapshot(pr_global(),0,0,true))return false;
 native_catalog_busy=true;
 unsigned count=0,accepted=0;
 bool valid=catalog_valid(&count);
 if(valid)for(unsigned j=0;j<count;j++){
  const uint8_t *r=catalog+16+j*NC_RECORD;
  struct rt865_launch_request request={.source={.read=catalog_read,.lba=nc_u32(r),.sectors=4095u,.bytes=nc_u32(r+4)}};
  for(unsigned k=0;k<32;k++)request.source.sha256[k]=r[8+k];
  for(unsigned k=0;k<8;k++)request.name[k]=(char)r[40+k];
  uint32_t pid;
  if(s865_native_launch(&request,&pid)!=PS_OK)break;
  ++accepted;
 }
 native_wipe(catalog,sizeof catalog);native_catalog_busy=false;
 arch_puts("STYLEOS NATIVE CATALOG");field("valid",valid);field("requested",count);field("accepted",accepted);field("resident",pr_snapshot(pr_global(),0,0,true));arch_putc('\n');
 return valid&&accepted==count;
}
#endif
