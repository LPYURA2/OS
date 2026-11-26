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
/* Only this acceptance translation unit sees the explicitly pinned stop catalog.
 * No automatic kill policy or key binding is added to the ordinary kernel. */
static unsigned stop_phase;
static uint32_t stop_pid,peer_pid;
static uint64_t stop_gen,peer_gen;
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
static enum ps_status launch_record(const uint8_t *r,uint32_t *pid){
 struct rt865_launch_request request={.source={.read=catalog_read,.lba=nc_u32(r),.sectors=4095u,.bytes=nc_u32(r+4)}};
 for(unsigned k=0;k<32;k++)request.source.sha256[k]=r[8+k];
 for(unsigned k=0;k<8;k++)request.name[k]=(char)r[40+k];
 return s865_native_launch(&request,pid);
}
static bool stop_fail(void){
 arch_puts("STYLEOS NATIVE FAIL forced-stop acceptance\n");return false;
}
static bool stop_step(void){
 const struct pr_entry *target=pr_find(pr_global(),stop_pid);
 const struct pr_entry *peer=pr_find(pr_global(),peer_pid);
 if(!target||!peer||target->state!=PR_READY||peer->state!=PR_READY||
    target->scheduler_handle!=stop_gen||peer->scheduler_handle!=peer_gen||
    target->cpu_ticks<2||peer->cpu_ticks<2||pr_snapshot(pr_global(),0,0,true)!=2)
  return stop_fail();
 uint64_t peer_ticks=peer->cpu_ticks,peer_root=peer->page_root;
 uint64_t old_root=target->page_root;
 uint32_t old_pid=stop_pid;uint64_t old_gen=stop_gen;
 uint32_t serial=pr_global()->change_serial;
 if(s865_native_stop(old_pid,old_gen,false)!=PS_EVIDENCE||
    s865_native_stop(old_pid,peer_gen,true)!=PS_EVIDENCE||
    pr_global()->change_serial!=serial||
    !pr_find(pr_global(),old_pid)||!pr_find(pr_global(),peer_pid))
  return stop_fail();
 arch_puts("STYLEOS NATIVE STOP-REFUSALS");field("pid",old_pid);field("gen",old_gen);arch_putc('\n');
 if(s865_native_stop(old_pid,old_gen,true)!=PS_OK||pr_find(pr_global(),old_pid)||
    s865_native_stop(old_pid,old_gen,true)!=PS_EVIDENCE||
    pr_snapshot(pr_global(),0,0,true)!=1)return stop_fail();
 peer=pr_find(pr_global(),peer_pid);
 if(!peer||peer->scheduler_handle!=peer_gen||peer->page_root!=peer_root||peer->cpu_ticks!=peer_ticks||
    peer->state!=PR_READY)return stop_fail();
 if(stop_phase==1){
  unsigned count=0;uint32_t fresh=0;
  if(!catalog_valid(&count)||count!=32||launch_record(catalog+16,&fresh)!=PS_OK){
   native_wipe(catalog,sizeof catalog);return stop_fail();
  }
  native_wipe(catalog,sizeof catalog);
  const struct pr_entry *restarted=pr_find(pr_global(),fresh);
  if(!restarted||fresh==old_pid||restarted->scheduler_handle==old_gen||
     restarted->page_root!=old_root||restarted->cpu_ticks||restarted->state!=PR_READY||
     pr_snapshot(pr_global(),0,0,true)!=2||
     s865_native_stop(fresh,old_gen,true)!=PS_EVIDENCE)return stop_fail();
  stop_pid=fresh;stop_gen=restarted->scheduler_handle;stop_phase=2;
  arch_puts("STYLEOS NATIVE STOP-RESTART");field("old-pid",old_pid);field("old-gen",old_gen);
  field("pid",stop_pid);field("gen",stop_gen);field("root",old_root);field("peer",peer_pid);
  field("peer-ticks",peer_ticks);arch_putc('\n');
  return true;
 }
 if(stop_phase!=2||s865_native_stop(peer_pid,peer_gen,true)!=PS_OK||
    pr_snapshot(pr_global(),0,0,true))return stop_fail();
 stop_phase=3;
 arch_puts("STYLEOS NATIVE STOP-DONE");field("resident",pr_snapshot(pr_global(),0,0,true));
 field("dispatches",runtime860->dispatcher.dispatches);arch_putc('\n');
 return true;
}
bool s865_native_catalog_request(void){
 if(native_catalog_busy||!s865_executor_idle())return false;
 if(stop_phase==1||stop_phase==2){
  native_catalog_busy=true;bool ok=stop_step();native_catalog_busy=false;return ok;
 }
 if(stop_phase||pr_snapshot(pr_global(),0,0,true))return false;
 native_catalog_busy=true;
 unsigned count=0,accepted=0;
 bool valid=catalog_valid(&count);
 static const uint8_t marker[8]={'s','t','o','p','0','0',0,0};
 bool forced=valid&&count==32&&nc_equal(catalog+16+40,marker,8);
 if(valid)for(unsigned j=0;j<count;j++){
  const uint8_t *r=catalog+16+j*NC_RECORD;
  uint32_t pid;
  if(launch_record(r,&pid)!=PS_OK)break;
  if(forced&&j<2){
   const struct pr_entry *owner=pr_find(pr_global(),pid);
   if(!owner)break;
   if(j==0){stop_pid=pid;stop_gen=owner->scheduler_handle;}
   else{peer_pid=pid;peer_gen=owner->scheduler_handle;}
  }
  ++accepted;
 }
 native_wipe(catalog,sizeof catalog);native_catalog_busy=false;
 arch_puts("STYLEOS NATIVE CATALOG");field("valid",valid);field("requested",count);field("accepted",accepted);field("resident",pr_snapshot(pr_global(),0,0,true));arch_putc('\n');
 if(forced&&accepted==count)stop_phase=1;
 return valid&&accepted==count;
}
#endif
