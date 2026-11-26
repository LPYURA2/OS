#include <styleos/stylefs_dircommit64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { SECTORS=2+16*32 };
static uint8_t disk[SECTORS][512],durable[SECTORS][512],saved[SECTORS][512];
static struct dc64_workspace work;
static unsigned writes,flushes,fail_write,fail_flush;static bool persist_failed;
static bool rd(void *c,uint64_t l,uint8_t *p){(void)c;if(l>=SECTORS)return false;memcpy(p,disk[l],512);return true;}
static bool wr(void *c,uint64_t l,const uint8_t *p){(void)c;assert(l<SECTORS);if(++writes==fail_write)return false;memcpy(disk[l],p,512);return true;}
static bool fl(void *c){(void)c;if(++flushes==fail_flush){if(persist_failed)memcpy(durable,disk,sizeof disk);return false;}memcpy(durable,disk,sizeof disk);return true;}
static uint64_t token;
static struct bs_service b;
static struct dc64_state s;
static void attach(void){
 memset(&b,0,sizeof b);uint64_t gen;
 struct bs_backend be={0,1,SECTORS,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK&&bs_grant_range(&b,gen,1,0,SECTORS,BS_READ|BS_WRITE,&token)==BS_OK);
}
static struct sd64_key key(uint64_t i){
 struct sd64_key k={.parent=1+i/40};
 int n=snprintf((char *)k.name,sizeof k.name,"entry-%04llu",(unsigned long long)i);
 assert(n>0&&n<SD64_NAME);k.length=(uint16_t)n;return k;
}
struct source{uint64_t delta,cancel;bool duplicate;};
static bool next(void *ctx,uint64_t i,struct sd64_record *r){
 struct source *a=ctx;if(i==a->cancel)return false;
 *r=(struct sd64_record){key(a->duplicate&&i==1?0:i),UINT64_C(0x100000000)+i+a->delta,SD64_FILE};
 return true;
}
static void check(uint64_t count,uint64_t delta){
 assert(s.count==count);
 for(uint64_t i=0;i<count;i++){
  struct sd64_key k=key(i);uint8_t p[SD64_PAGE],kind=0;uint64_t obj=0;
  assert(sd64_lookup(&b,1,token,&s.root,&k,p,&obj,&kind)==SD64_OK);
  assert(obj==UINT64_C(0x100000000)+i+delta&&kind==SD64_FILE);
 }
}
static void reopen(void){
 memcpy(disk,durable,sizeof disk);writes=flushes=fail_write=fail_flush=0;persist_failed=false;
 attach();assert(dc64_mount(&s,&b,1,token,SECTORS,&work)==DC64_OK);
}
int main(void){
 attach();assert(dc64_mount(&s,&b,1,token,SECTORS,&work)==DC64_CORRUPT);
 assert(dc64_provision(&b,1,token,SECTORS)==DC64_OK);
 assert(dc64_provision(&b,1,token,SECTORS)==DC64_INVALID);
 reopen();assert(s.count==0&&s.epoch==1&&!s.root.sector);
 struct source a={0,UINT64_MAX,false},z={1000,UINT64_MAX,false};
 assert(dc64_publish(&s,&work,40,next,&a)==DC64_OK);check(40,0);
 reopen();check(40,0);assert(s.epoch==2);
 memcpy(saved,durable,sizeof saved);
 assert(dc64_publish(&s,&work,57,next,&z)==DC64_OK);check(57,1000);
 unsigned operations=writes;assert(operations==flushes&&operations==50);
 reopen();check(57,1000);assert(s.epoch==3);
 /* Every write failure and both possible durability outcomes of every
  * failed flush; cold boot sees complete old or complete new snapshot. */
 for(unsigned mode=0;mode<3;mode++)for(unsigned i=1;i<=operations;i++){
  memcpy(durable,saved,sizeof durable);reopen();
  if(mode==0)fail_write=i;else{fail_flush=i;persist_failed=mode==2;}
  assert(dc64_publish(&s,&work,57,next,&z)==DC64_UNCERTAIN&&!s.valid);
  reopen();if(s.epoch==2)check(40,0);else{assert(s.epoch==3);check(57,1000);}
  if(mode<2||i<operations)assert(s.epoch==2);
 }
 memcpy(durable,saved,sizeof durable);reopen();
 z.cancel=16;assert(dc64_publish(&s,&work,57,next,&z)==DC64_CANCELLED);check(40,0);
 reopen();check(40,0);z.cancel=UINT64_MAX;
 z.duplicate=true;assert(dc64_publish(&s,&work,57,next,&z)==DC64_INVALID);
 reopen();check(40,0);z.duplicate=false;
 unsigned old=writes;assert(dc64_publish(&s,&work,UINT64_MAX,next,&z)==DC64_FULL&&writes==old);
 assert(dc64_publish(&s,&work,57,next,&z)==DC64_OK);reopen();check(57,1000);
 durable[s.root.sector][300]^=1;reopen();assert(s.epoch==2);check(40,0);
 assert(dc64_publish(&s,&work,0,0,0)==DC64_OK);reopen();assert(s.count==0&&!s.root.sector);
 puts("STYLEFS 22 DIRECTORY COMMIT PASS: 150 interrupted writes/flushes, cancellation, corruption fallback, empty tree");
 return 0;
}
