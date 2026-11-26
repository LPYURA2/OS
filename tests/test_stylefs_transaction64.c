#include <styleos/stylefs_transaction64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { DIR=258,DATA=4096,TOTAL=2+DIR+DATA };
static uint8_t disk[TOTAL][512],saved[TOTAL][512],durable[TOTAL][512];
static struct bs_service b;
static struct tx64_state tx;
static struct fb64_workspace w;
static struct ns64_node nodes[100];
static struct cp64_layout l;
static struct dc64_state d;
static struct sfs64_journal j;
static struct ns64_proof p;
static struct fb64_view v;
static struct cp64_result receipt;
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
static void open_ok(void){assert(tx64_open(&tx,&b,&l,nodes,100,&w)==TX64_OK);}
static struct sd64_record a={.key={.parent=1,.length=1,.name={'a'}},.target=2,.kind=SD64_FILE},
 second={.key={.parent=1,.length=1,.name={'b'}},.target=3,.kind=SD64_FILE};
static bool source(void *ctx,uint64_t off,uint8_t *buf,size_t n){
 if(ctx)return false;
 assert(tx64_create(&tx,&w,&second,0,0,0)==TX64_LOCKED);
 for(size_t i=0;i<n;i++)buf[i]=(uint8_t)((off+i)^0x55);
 return true;
}
static void recover(void){
 uint64_t before=writes;
 assert(cp64_recover(&b,&l,&d,&j,&p,nodes,100,&w.reader,&v,&receipt)==CP64_OK&&writes==before);
 assert(receipt.sequence==1||receipt.sequence==2);
 assert(d.count==receipt.sequence-1);
 if(d.count){
  struct sfs64_file file;
  assert(fb64_stat(&v,(const uint8_t *)"/a",2,w.reader.page,&file)==FB64_OK&&file.bytes==1041);
 }
}
int main(void){
 attach(true);
 assert(dc64_provision(&b,7,l.dir,DIR)==DC64_OK);
 assert(sfs64_journal_provision(&b,7,l.data,DATA,32)==SJ_OK);
 assert(tx64_open(&tx,&b,&l,nodes,100,&w)!=TX64_OK);
 assert(dc64_mount(&d,&b,7,l.dir,DIR,&w.writer)==DC64_OK);
 assert(sfs64_journal_mount(&j,&b,7,l.data,DATA,32)==SJ_OK);
 assert(ns64_validate(&d,&w.reader,nodes,100,&p)==NS64_OK);
 assert(fb64_bind(&v,&d,&p,&j)==FB64_OK&&cp64_save(&b,&l,&v)==CP64_OK);
 open_ok();memcpy(saved,durable,sizeof saved);
 uint64_t start=writes;
 assert(tx64_create(&tx,&w,&a,1041,source,(void *)1)==TX64_CANCELLED&&tx.ready&&writes==start);
 assert(tx64_create(&tx,&w,&a,1041,source,0)==TX64_OK&&tx.ready&&tx.receipt.sequence==2);
 uint64_t operations=writes-start;assert(operations==23);
 reboot(true);open_ok();assert(tx.receipt.sequence==2);
 assert(tx64_create(&tx,&w,&second,0,0,0)==TX64_OK&&tx.receipt.sequence==3);
 reboot(true);open_ok();assert(tx.dir.count==2);
 for(unsigned mode=0;mode<3;mode++)for(uint64_t i=1;i<=operations;i++){
  memcpy(durable,saved,sizeof saved);reboot(true);open_ok();
  if(!mode)fail_write=writes+i;else{fail_flush=flushes+i;persist_failed=mode==2;}
  assert(tx64_create(&tx,&w,&a,1041,source,0)==TX64_RECOVERY&&!tx.ready);
  uint64_t before=writes;assert(tx64_create(&tx,&w,&second,0,0,0)==TX64_LOCKED&&writes==before);
  reboot(false);recover();
  if(mode!=2||i<operations)assert(receipt.sequence==1);
  else assert(receipt.sequence==2);
  /* Reopening writable is refused if raw latest pair differs from receipt. */
  reboot(true);enum tx64_status rc=tx64_open(&tx,&b,&l,nodes,100,&w);
  if(rc==TX64_OK)assert(tx.receipt.sequence==receipt.sequence);
  else assert(!tx.ready);
 }
 puts("STYLEFS 28 TRANSACTION PASS: 69 write/flush failure cases, checkpoint-last acknowledgement, reentry refusal and write lock");
 return 0;
}
