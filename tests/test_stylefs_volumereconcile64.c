#include <styleos/stylefs_widevolume64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { DIR=258,INO=258,DATA=2048,TOTAL=2+DIR+INO+DATA };
static uint8_t disk[TOTAL][512],durable[TOTAL][512],saved[TOTAL][512],pending[TOTAL][512];
static struct bs_service b;
static struct wv64_state s;
static struct wv64_workspace w;
static struct ns64_node nodes[100];
static struct ow64_extent entries[2][100];
static struct sfs64_run runs[2][100];
static struct ow64_inventory inv[2]={{entries[0],runs[0],100,0},{entries[1],runs[1],100,0}};
static struct wv64_layout l;
static uint64_t gen,writes,flushes,fail_write=UINT64_MAX,fail_flush=UINT64_MAX;
static bool persist_failed;
static bool rd(void *ctx,uint64_t a,uint8_t *out){(void)ctx;if(a>=TOTAL)return false;memcpy(out,disk[a],512);return true;}
static bool wr(void *ctx,uint64_t a,const uint8_t *in){(void)ctx;if(++writes==fail_write||a>=TOTAL)return false;memcpy(disk[a],in,512);return true;}
static bool fl(void *ctx){(void)ctx;if(++flushes==fail_flush){if(persist_failed)memcpy(durable,disk,sizeof disk);return false;}memcpy(durable,disk,sizeof disk);return true;}
static void attach(bool write){
 struct bs_backend be={0,1,TOTAL,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK);
 unsigned rights=BS_READ|(write?BS_WRITE:0);
 l=(struct wv64_layout){.owner=7,.dir_sectors=DIR,.inode_sectors=INO,.data_sectors=DATA,.data_start=32};
 assert(bs_grant_range(&b,gen,7,0,2,rights,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2,DIR,rights,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2+DIR,INO,rights,&l.inode)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2+DIR+INO,DATA,rights,&l.data)==BS_OK);
}
static void reboot(bool write){
 assert(bs_detach(&b,gen)==BS_OK);memcpy(disk,durable,sizeof disk);
 fail_write=fail_flush=UINT64_MAX;persist_failed=false;attach(write);
}
static enum wv64_status open_volume(bool write){return wv64_open(&s,&b,&l,write,nodes,100,&w,inv);}
static struct sd64_record a={.key={.parent=1,.length=1,.name={'a'}},.target=2,.kind=SD64_FILE},
 second={.key={.parent=1,.length=1,.name={'b'}},.target=3,.kind=SD64_FILE};
static bool source(void *ctx,uint64_t off,uint8_t *out,size_t n){
 if(ctx)return false;
 /* Trusted test source, no reentry. */
 for(size_t i=0;i<n;i++)out[i]=(uint8_t)((off+i)^0x5a);
 return true;
}


static enum wv64_status repair(void){return wv64_reconcile(&s,&b,&l,nodes,100,&w,inv);}
static void verify(void){
 struct sfs64_file file;assert(s.ready&&s.sequence==2);
 assert(wv64_stat(&s,&w,(const uint8_t *)"/a",2,&file)==WV64_OK&&file.bytes==1041);
 assert(wv64_stat(&s,&w,(const uint8_t *)"/b",2,&file)==WV64_MISSING);
 uint8_t out[1041];size_t done;
 assert(wv64_read(&s,&w,(const uint8_t *)"/a",2,2,1,0,out,sizeof out,&done)==WV64_OK&&done==sizeof out);
 for(size_t i=0;i<done;i++)assert(out[i]==(uint8_t)(i^0x5a));
}
int main(void){
 attach(true);
 assert(dc64_provision(&b,7,l.dir,DIR)==DC64_OK);
 assert(ic64_provision(&b,7,l.inode,INO,DATA,32)==IC64_OK);
 assert(wv64_provision_checkpoint(&b,&l,&w)==WV64_OK);
 assert(open_volume(true)==WV64_OK);
 assert(wv64_create(&s,&w,inv,&a,1041,source,0)==WV64_OK);
 memcpy(saved,durable,sizeof saved);
 /* Simulate pending inode + directory roots without a common receipt. */
 assert(is64_create(&s.inode,l.data,&w.stream,inv,3,1041,source,0)==IS64_OK);
 struct dc64_edit add={.op=DC64_INSERT,.entry=second};
 assert(ns64_apply(&s.dir,&s.proof,&w.writer,&w.reader,&add)==NS64_OK);
 memcpy(pending,durable,sizeof pending);
 reboot(false);assert(open_volume(false)==WV64_OK);verify();
 reboot(true);assert(open_volume(true)==WV64_RECOVERY);
 uint64_t before=writes;assert(repair()==WV64_OK&&writes-before==2);verify();
 assert(wv64_create(&s,&w,inv,&second,0,0,0)==WV64_OK&&s.sequence==3);
 for(unsigned mode=0;mode<3;mode++)for(unsigned i=1;i<=2;i++){
  memcpy(durable,pending,sizeof pending);reboot(true);
  if(!mode)fail_write=writes+i;else{fail_flush=flushes+i;persist_failed=mode==2;}
  assert(repair()==WV64_RECOVERY&&!s.ready);
  reboot(true);assert(repair()==WV64_OK);verify();
 }
 /* Directory-only pending rename: discard precisely one root. */
 memcpy(durable,saved,sizeof saved);reboot(true);assert(open_volume(true)==WV64_OK);
 struct dc64_edit rename={.op=DC64_RENAME,.entry=a,.destination={.parent=1,.length=1,.name={'x'}}};
 assert(ns64_apply(&s.dir,&s.proof,&w.writer,&w.reader,&rename)==NS64_OK);
 reboot(true);before=writes;assert(repair()==WV64_OK&&writes-before==1);verify();
 /* Inode-only pending create: discard precisely one root. */
 memcpy(durable,saved,sizeof saved);reboot(true);assert(open_volume(true)==WV64_OK);
 assert(is64_create(&s.inode,l.data,&w.stream,inv,3,0,0,0)==IS64_OK);
 reboot(true);before=writes;assert(repair()==WV64_OK&&writes-before==1);verify();
 /* Idempotent, no writes on clean volume. */
 before=writes;assert(repair()==WV64_OK&&writes==before);verify();
 /* Damaged newest common receipt: do not roll back writable. */
 memcpy(durable,pending,sizeof pending);durable[1][0]^=1;reboot(true);
 before=writes;assert(repair()==WV64_CORRUPT&&writes==before&&!s.ready);
 memcpy(durable,pending,sizeof pending);reboot(false);
 before=writes;assert(repair()==WV64_INVALID&&writes==before);
 puts("STYLEFS 37 WIDE RECONCILE PASS: retained bytes, two/one-root repair, six repair cuts, idempotence and damaged checkpoint refusal");
 return 0;
}
