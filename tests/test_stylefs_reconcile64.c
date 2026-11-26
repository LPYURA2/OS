#include <styleos/stylefs_transaction64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { DIR=258,DATA=4096,TOTAL=2+DIR+DATA };
static uint8_t disk[TOTAL][512],durable[TOTAL][512],saved[TOTAL][512],pending[TOTAL][512];
static struct bs_service b;
static struct tx64_state tx;
static struct dc64_state d;
static struct sfs64_journal j;
static struct ns64_proof p;
static struct ns64_node nodes[100];
static struct fb64_workspace w;
static struct fb64_view v;
static struct cp64_layout l;
static struct cp64_result result;
static uint64_t gen,writes,flushes,fail_write=UINT64_MAX,fail_flush=UINT64_MAX;
static bool persist_failed;
static bool rd(void *ctx,uint64_t a,uint8_t *out){(void)ctx;if(a>=TOTAL)return false;memcpy(out,disk[a],512);return true;}
static bool wr(void *ctx,uint64_t a,const uint8_t *in){(void)ctx;if(++writes==fail_write||a>=TOTAL)return false;memcpy(disk[a],in,512);return true;}
static bool fl(void *ctx){(void)ctx;if(++flushes==fail_flush){if(persist_failed)memcpy(durable,disk,sizeof disk);return false;}memcpy(durable,disk,sizeof disk);return true;}
static void attach(bool write){
 struct bs_backend be={0,1,TOTAL,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK);
 unsigned rights=BS_READ|(write?BS_WRITE:0);
 l=(struct cp64_layout){.owner=7,.dir_sectors=DIR,.data_sectors=DATA,.data_start=32};
 assert(bs_grant_range(&b,gen,7,0,2,rights,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2,DIR,rights,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2+DIR,DATA,rights,&l.data)==BS_OK);
}
static void reboot(bool write){
 assert(bs_detach(&b,gen)==BS_OK);memcpy(disk,durable,sizeof disk);
 fail_write=fail_flush=UINT64_MAX;persist_failed=false;attach(write);
}
static void mount(void){
 assert(dc64_mount(&d,&b,7,l.dir,DIR,&w.writer)==DC64_OK);
 assert(sfs64_journal_mount(&j,&b,7,l.data,DATA,32)==SJ_OK);
 assert(ns64_validate(&d,&w.reader,nodes,100,&p)==NS64_OK);
 assert(fb64_bind(&v,&d,&p,&j)==FB64_OK);
}
static struct sd64_record a={.key={.parent=1,.length=1,.name={'a'}},.target=2,.kind=SD64_FILE},
 second={.key={.parent=1,.length=1,.name={'b'}},.target=3,.kind=SD64_FILE};
static bool source(void *ctx,uint64_t off,uint8_t *buf,size_t n){
 (void)ctx;for(size_t i=0;i<n;i++)buf[i]=(uint8_t)((off+i)^0x55);return true;
}
static enum cp64_status repair(void){return cp64_reconcile(&b,&l,&d,&j,&p,nodes,100,&w.reader,&v,&result);}
static void verify_old(void){
 assert(result.sequence==2&&d.count==1&&v.valid);
 struct sfs64_file file;
 assert(fb64_stat(&v,(const uint8_t *)"/a",2,w.reader.page,&file)==FB64_OK&&file.bytes==1041);
 uint8_t buf[1041];size_t done=0;
 assert(sfs64_stream_read(&j,2,1,0,buf,sizeof buf,&done)==BS_OK&&done==sizeof buf);
 for(size_t i=0;i<done;i++)assert(buf[i]==(uint8_t)(i^0x55));
 assert(sfs64_stream_stat(&j,3,&file)==SJ_STALE);
}
int main(void){
 attach(true);assert(dc64_provision(&b,7,l.dir,DIR)==DC64_OK);
 assert(sfs64_journal_provision(&b,7,l.data,DATA,32)==SJ_OK);
 mount();assert(cp64_save(&b,&l,&v)==CP64_OK);
 assert(tx64_open(&tx,&b,&l,nodes,100,&w)==TX64_OK);
 assert(tx64_create(&tx,&w,&a,1041,source,0)==TX64_OK);
 memcpy(saved,durable,sizeof saved);
 /* File and name both durable, receipt has NOT been committed. */
 mount();assert(fb64_create(&v,&w,&second,1041,source,0)==FB64_OK);
 memcpy(pending,durable,sizeof pending);
 reboot(true);assert(tx64_open(&tx,&b,&l,nodes,100,&w)!=TX64_OK);
 uint64_t before=writes;assert(repair()==CP64_OK&&writes-before==2);verify_old();
 reboot(true);assert(tx64_open(&tx,&b,&l,nodes,100,&w)==TX64_OK);
 assert(tx64_create(&tx,&w,&second,1041,source,0)==TX64_OK&&tx.receipt.sequence==3);
 /* Interrupt both header invalidations, with both flush outcomes. */
 for(unsigned mode=0;mode<3;mode++)for(unsigned i=1;i<=2;i++){
  memcpy(durable,pending,sizeof pending);reboot(true);
  if(!mode)fail_write=writes+i;else{fail_flush=flushes+i;persist_failed=mode==2;}
  assert(repair()==CP64_UNCERTAIN&&!v.valid);
  reboot(true);assert(repair()==CP64_OK);verify_old();
  assert(tx64_open(&tx,&b,&l,nodes,100,&w)==TX64_OK);
 }
 /* One-sided pending commit: only data header is discarded. */
 memcpy(durable,saved,sizeof saved);reboot(true);mount();
 w.stream=(struct sfs64_stream){0};assert(sfs64_stream_begin(&w.stream,&j,3,0,0)==SJ_OK);
 assert(sfs64_stream_finish(&w.stream)==SJ_OK);reboot(true);
 before=writes;assert(repair()==CP64_OK&&writes-before==1);verify_old();
 /* A nonblank damaged checkpoint may be an acknowledged transaction. */
 memcpy(durable,pending,sizeof pending);durable[1][0]^=1;reboot(true);
 before=writes;assert(repair()==CP64_CORRUPT&&writes==before&&!v.valid);
 /* Read-only grant refuses any repair. */
 memcpy(durable,pending,sizeof pending);reboot(false);
 before=writes;assert(repair()==CP64_INVALID&&writes==before);
 puts("STYLEFS 29 RECONCILE PASS: confirmed bytes retained, two/one-header rollback, 6 interrupted repairs, writable reopen, damaged receipt refusal");
 return 0;
}
