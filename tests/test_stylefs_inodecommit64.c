#include <styleos/stylefs_inodecommit64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { SECTORS=2+16*32 };
#define CAP UINT64_C(8589934592)
static uint8_t disk[SECTORS][512],durable[SECTORS][512],saved[SECTORS][512];
static struct bs_service b;
static struct ic64_state s;
static struct ic64_workspace w;
static uint64_t gen,token,writes,flushes,fail_write=UINT64_MAX,fail_flush=UINT64_MAX;
static bool persist_failed;
static bool rd(void *ctx,uint64_t at,uint8_t *p){(void)ctx;if(at>=SECTORS)return false;memcpy(p,disk[at],512);return true;}
static bool wr(void *ctx,uint64_t at,const uint8_t *p){(void)ctx;if(++writes==fail_write||at>=SECTORS)return false;memcpy(disk[at],p,512);return true;}
static bool fl(void *ctx){(void)ctx;if(++flushes==fail_flush){if(persist_failed)memcpy(durable,disk,sizeof disk);return false;}memcpy(durable,disk,sizeof disk);return true;}
static void attach(void){
 struct bs_backend be={0,1,SECTORS,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK&&bs_grant_range(&b,gen,7,0,SECTORS,BS_READ|BS_WRITE,&token)==BS_OK);
}
static void reboot(void){
 assert(bs_detach(&b,gen)==BS_OK);memcpy(disk,durable,sizeof disk);
 fail_write=fail_flush=UINT64_MAX;persist_failed=false;attach();
 assert(ic64_mount(&s,&b,7,token,SECTORS,CAP,32,&w)==IC64_OK);
}
struct source {uint64_t revision,cancel;bool duplicate;};
static bool next(void *ctx,uint64_t i,uint8_t *out,size_t limit,size_t *n){
 struct source *x=ctx;
 if(i==x->cancel)return false;
 uint64_t id=UINT64_C(0x100000000)+(x->duplicate?0:i);
 struct sfs64_file f={id,x->revision,511,1};
 struct sfs64_run run={UINT64_C(0x100010000)+i,1};
 assert(limit>=80);*n=80;return sfs64_map_encode(out,*n,f,CAP,32,&run,1);
}
static void verify(uint64_t count,uint64_t revision){
 assert(s.table.count==count);
 for(uint64_t i=0;i<count;i++){
  struct sfs64_map map;
  assert(it64_lookup(&s.table,s.table.geometry.epoch,UINT64_C(0x100000000)+i,w.page,&map)==IT64_OK&&map.file.revision==revision);
 }
}
int main(void){
 attach();assert(ic64_provision(&b,7,token,SECTORS,CAP,32)==IC64_OK);reboot();
 struct source first={1,UINT64_MAX,false},second={2,UINT64_MAX,false};
 assert(ic64_publish(&s,&w,100,next,&first)==IC64_OK);verify(100,1);
 memcpy(saved,durable,sizeof saved);struct ic64_state stale=s;
 uint64_t before=writes;
 assert(ic64_publish(&s,&w,150,next,&second)==IC64_OK);verify(150,2);
 uint64_t operations=writes-before;assert(operations==34);
 before=writes;assert(ic64_publish(&stale,&w,1,next,&first)==IC64_STALE&&writes==before);
 reboot();verify(150,2);
 for(unsigned mode=0;mode<3;mode++)for(uint64_t i=1;i<=operations;i++){
  memcpy(durable,saved,sizeof saved);reboot();
  if(!mode)fail_write=writes+i;else{fail_flush=flushes+i;persist_failed=mode==2;}
  assert(ic64_publish(&s,&w,150,next,&second)==IC64_UNCERTAIN&&!s.table.valid);
  reboot();
  if(mode==2&&i==operations)verify(150,2);else verify(100,1);
 }
 memcpy(durable,saved,sizeof saved);reboot();second.cancel=55;
 assert(ic64_publish(&s,&w,150,next,&second)==IC64_CANCELLED);reboot();verify(100,1);
 second.cancel=UINT64_MAX;second.duplicate=true;
 assert(ic64_publish(&s,&w,150,next,&second)==IC64_INVALID);reboot();verify(100,1);
 second.duplicate=false;assert(ic64_publish(&s,&w,150,next,&second)==IC64_OK);
 durable[s.table.geometry.start][100]^=1;reboot();verify(100,1);
 assert(ic64_publish(&s,&w,0,0,0)==IC64_OK);reboot();assert(!s.table.count);
 printf("STYLEFS 31 INODE COMMIT PASS: 100 to 150 manifests, %llu interrupted writes/flushes, cancellation, stale handle and fallback\n",(unsigned long long)(operations*3));
 return 0;
}
