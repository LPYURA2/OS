/* Ordinary native runtime: reuse the same audited detached-root and
 * hardware execution path without the historical batch driver. Included
 * only by the owned-root architecture after all root helpers are defined. */
#include "native_executor_865.h"
#include <styleos/native_runtime_865.h>
static struct s846_root native_roots[S846_ROOT_COUNT];
static struct s844_lease native_leases[S846_ROOT_COUNT];
static struct s844_arena *native_arena;
static const struct boot_memory_evidence *native_evidence;
static struct s846_guard native_guard;
static struct s842_cache native_cache;
static bool native_ready;
static uint8_t native_scratch[8192];
static void native_wipe(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static bool native_audit(void *unused){
 (void)unused;
 return native_arena&&native_evidence&&read_cr3()==composed.built.root&&!(flags()&0x200)&&same_cache(&native_cache)&&
  rt865_roots_audit_sparse_frozen(base861,&native_cache,&frozen861,native_evidence,native_arena,native_leases,&native_guard,&composed,native_roots);
}
static bool native_release(void *unused,unsigned id,uint64_t root,uint64_t generation){
 (void)unused;
 if(id>=S846_ROOT_COUNT||!native_audit(0)||native_roots[id].built.root!=root||
    native_leases[id].generation!=generation||!s844_lease_live(native_arena,native_leases[id]))return false;
 struct s844_lease lease=native_leases[id];
 native_wipe(&native_roots[id],sizeof native_roots[id]);
 native_wipe(child_tables861[id],sizeof child_tables861[id]);
 if(!s844_release(native_arena,lease.owner,lease))return false;
 native_wipe(&native_leases[id],sizeof native_leases[id]);
 return native_audit(0);
}
static bool native_boot(struct pg_experiment *p,const struct s842_cache *cache,const struct s846_guard *guard,struct s844_arena *arena){
 if(native_ready||!p||!p->evidence||!cache||!guard||!arena||!base861)return false;
 native_arena=arena;native_evidence=p->evidence;native_guard=*guard;native_cache=*cache;
 if(!native_audit(0)||!s865_executor_boot(composed.built.root,base861->lapic,native_audit,0))return false;
 native_ready=true;
 arch_puts("STYLEOS NATIVE RUNTIME READY");field("slots",S846_ROOT_COUNT);field("pages",S844_PAGES);field("root",read_cr3());arch_putc('\n');
 return true;
}
enum ps_status s865_native_launch(const struct rt865_launch_request *request,uint32_t *pid){
 if(!request||!pid)return PS_ARGUMENT;
 if(!native_ready||!s865_executor_idle()||!native_audit(0))return PS_STATE;
 if(pr_global()->change_serial>UINT32_MAX-128u)return PS_EXHAUSTED;
 unsigned id=0;
 while(id<S846_ROOT_COUNT&&native_leases[id].generation)++id;
 if(id==S846_ROOT_COUNT)return PS_FULL;
 if(!s844_acquire(native_arena,S846_OWNER_A+id,3,&native_leases[id]))return PS_FULL;
 if(!s865_root_build(base861,&native_cache,native_arena,native_leases,id,&native_guard,child_tables861[id],
    (uintptr_t)child_tables861[id],&native_roots[id])){
  native_wipe(&native_roots[id],sizeof native_roots[id]);
  native_wipe(child_tables861[id],sizeof child_tables861[id]);
  if(!s844_release(native_arena,native_leases[id].owner,native_leases[id]))fail("native-build-rollback");
  native_wipe(&native_leases[id],sizeof native_leases[id]);
  if(!native_audit(0))fail("native-build-audit");
  return PS_EVIDENCE;
 }
 struct nte_loaded image={0};uint32_t assigned=0;
 enum ps_status result=s865_executor_launch(id,&native_roots[id],native_arena,&native_leases[id],request,
    native_scratch,sizeof native_scratch,native_audit,0,&image,&assigned);
 if(result!=PS_OK){
  if(!native_release(0,id,native_roots[id].built.root,native_leases[id].generation))fail("native-launch-rollback");
  return result;
 }
 if(pr_account(pr_global(),assigned,native_leases[id].generation,0,3*4096)!=PR_OK||
    !s865_executor_idle()||!native_audit(0))fail("native-launch-publish");
 *pid=assigned;
 arch_puts("STYLEOS NATIVE ADMIT");field("slot",id);field("pid",assigned);field("gen",native_leases[id].generation);field("root",native_roots[id].built.root);field("entry",image.entry);arch_putc('\n');
 return PS_OK;
}
void s865_native_pump(void){
 if(!native_ready)fail("native-runtime-not-ready");
 if(!pr_snapshot(pr_global(),0,0,true))return;
 s865_executor_poll(native_audit,native_release,0);
 if(!pr_snapshot(pr_global(),0,0,true)){
  unsigned used;
  if(!s844_count(native_arena,&used)||used||!native_audit(0))fail("native-empty");
  arch_puts("STYLEOS NATIVE EMPTY");field("used",used);field("dispatches",runtime860->dispatcher.dispatches);arch_putc('\n');
 }
}

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
 if(native_catalog_busy||!native_ready||!s865_executor_idle()||
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
