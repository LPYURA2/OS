
#include <styleos/stylefs_inode_root64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {N=2+16*8,DC=1000,DS=19};
static uint8_t disk[N][512],durable[N][512],saved[N][512];
static struct bs_service b;
static struct ir64_state s,old;
static struct ip64_workspace w;
static struct sfs64_run runs[128];
static uint64_t token;
static unsigned writes,flushes,fail_write,fail_flush,fail_read,reads;
static bool persist_failed;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N||++reads==fail_read)return false;memcpy(p,disk[n],512);return true;}
static bool wr(void *c,uint64_t n,const uint8_t *p){(void)c;assert(n<N);if(++writes==fail_write)return false;memcpy(disk[n],p,512);return true;}
static bool fl(void *c){(void)c;if(++flushes==fail_flush){if(persist_failed)memcpy(durable,disk,sizeof disk);return false;}memcpy(durable,disk,sizeof disk);return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t gen;
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK&&bs_grant_range(&b,gen,7,0,N,3,&token)==BS_OK);
}
static void reopen(void){
 memcpy(disk,durable,sizeof disk);writes=flushes=reads=fail_write=fail_flush=fail_read=0;persist_failed=false;
 attach();assert(ir64_mount(&s,&b,7,token,N,DC,DS,&w,runs,128)==IP64_OK);
}
struct source {uint64_t offset,cancel;bool overlap;};
static bool next(void *ctx,uint64_t i,uint8_t *out,size_t cap,size_t *n){
 struct source *c=ctx;if(i==c->cancel)return false;
 struct sfs64_file f={i+2,1,512,42};
 struct sfs64_run run={DS+c->offset+(c->overlap?0:i),1};
 *n=sfs64_map_length(1);assert(*n<=cap);
 return sfs64_map_encode(out,*n,f,DC,DS,&run,1);
}
static void check(uint64_t e,uint64_t count){
 assert(s.valid&&s.root.epoch==e&&s.root.files==count);
 for(uint64_t i=0;i<count;i++){
  struct sfs64_file f={0};
  assert(ip64_stat(&b,7,token,&s.root,i+2,&w,&f)==IP64_OK&&f.id==i+2&&f.bytes==512);
 }
}
int main(void){
 attach();assert(ir64_mount(&s,&b,7,token,N,DC,DS,&w,runs,128)==IP64_CORRUPT);
 assert(ir64_provision(&b,7,token,N,DC,DS)==IP64_OK);
 assert(ir64_provision(&b,7,token,N,DC,DS)==IP64_INVALID);
 reopen();check(1,0);
 struct source a={0,UINT64_MAX,false},z={200,UINT64_MAX,false};
 assert(ir64_publish(&s,&w,runs,128,4,next,&a)==IP64_OK);reopen();check(2,4);
 memcpy(saved,durable,sizeof saved);old=s;
 assert(ir64_publish(&s,&w,runs,128,60,next,&z)==IP64_OK);
 unsigned ops=writes;assert(ops==18&&ops==flushes);check(3,60);
 assert(ir64_publish(&old,&w,runs,128,0,0,0)==IP64_INVALID&&!old.valid&&writes==ops);
 reopen();check(3,60);
 assert(ir64_mount_epoch(&old,&b,7,token,N,DC,DS,2,&w,runs,128)==IP64_OK&&old.root.files==4);
 assert(ir64_mount_epoch(&old,&b,7,token,N,DC,DS,1,&w,runs,128)==IP64_CORRUPT);
 assert(ir64_mount(&old,&b,7,token,N,DC,DS,&w,runs,4)==IP64_FULL&&!old.valid);
 for(unsigned mode=0;mode<3;mode++)for(unsigned i=1;i<=ops;i++){
  memcpy(durable,saved,sizeof durable);reopen();
  if(!mode)fail_write=i;else{fail_flush=i;persist_failed=mode==2;}
  assert(ir64_publish(&s,&w,runs,128,60,next,&z)==IP64_UNCERTAIN&&!s.valid);
  reopen();if(mode==2&&i==ops)check(3,60);else check(2,4);
 }
 memcpy(durable,saved,sizeof durable);reopen();
 z.cancel=50;assert(ir64_publish(&s,&w,runs,128,60,next,&z)==IP64_CANCELLED);reopen();check(2,4);
 z.cancel=UINT64_MAX;z.overlap=true;
 assert(ir64_publish(&s,&w,runs,128,60,next,&z)==IP64_CORRUPT);reopen();check(2,4);
 z.overlap=false;
 assert(ir64_publish(&s,&w,runs,128,60,next,&z)==IP64_OK);reopen();check(3,60);
 durable[s.root.start][63]^=1;reopen();check(2,4);
 assert(ir64_mount_epoch(&old,&b,7,token,N,DC,DS,3,&w,runs,128)==IP64_CORRUPT);
 assert(ir64_publish(&s,&w,runs,128,0,0,0)==IP64_OK);reopen();check(3,0);
 fail_read=reads+1;
 assert(ir64_mount(&old,&b,7,token,N,DC,DS,&w,runs,128)==IP64_IO&&!old.valid);
 puts("STYLEFS 29 INODE ROOT PASS: 54 write/flush cuts, exact epoch, stale writer, capacity and corruption refusal, fallback");
 return 0;
}
