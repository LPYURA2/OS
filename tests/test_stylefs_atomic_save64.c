#include <styleos/stylefs_widevolume64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { DIR=258,INO=258,DATA=2048,TOTAL=2+DIR+INO+DATA };
static uint8_t disk[TOTAL][512],durable[TOTAL][512],saved[TOTAL][512];
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



static bool changed(void *ctx,uint64_t off,uint8_t *out,size_t n){
 if(ctx&&off>=SFS64_WINDOW_BYTES)return false;
 for(size_t i=0;i<n;i++)out[i]=(uint8_t)((off+i)^0xa7);
 return true;
}
static enum wv64_status replace(uint64_t rev,uint64_t bytes,is64_source src,void *ctx){
 return wv64_replace(&s,&w,inv,(const uint8_t *)"/a",2,2,rev,bytes,src,ctx);
}
static void verify(uint64_t rev,uint64_t bytes){
 struct sfs64_file file;uint8_t out[2048];size_t done=0;
 assert(s.ready&&s.dir_epoch==3);
 assert(wv64_stat(&s,&w,(const uint8_t *)"/a",2,&file)==WV64_OK&&file.id==2&&file.revision==rev&&file.bytes==bytes);
 assert(wv64_read(&s,&w,(const uint8_t *)"/a",2,2,rev,0,out,sizeof out,&done)==WV64_OK&&done==bytes);
 for(size_t i=0;i<done;i++)assert(out[i]==(uint8_t)(i^(rev==1?0x5a:0xa7)));
 assert(wv64_stat(&s,&w,(const uint8_t *)"/b",2,&file)==WV64_OK&&file.id==3&&file.revision==1&&file.bytes==1041);
 assert(wv64_read(&s,&w,(const uint8_t *)"/b",2,3,1,0,out,sizeof out,&done)==WV64_OK&&done==1041);
 for(size_t i=0;i<done;i++)assert(out[i]==(uint8_t)(i^0x5a));
}
int main(void){
 attach(true);
 assert(dc64_provision(&b,7,l.dir,DIR)==DC64_OK);
 assert(ic64_provision(&b,7,l.inode,INO,DATA,32)==IC64_OK);
 assert(wv64_provision_checkpoint(&b,&l,&w)==WV64_OK);
 assert(open_volume(true)==WV64_OK);
 assert(wv64_create(&s,&w,inv,&a,1041,source,0)==WV64_OK);
 assert(wv64_create(&s,&w,inv,&second,1041,source,0)==WV64_OK);
 verify(1,1041);
 uint64_t before=writes;
 assert(replace(2,1501,changed,0)==WV64_CONFLICT&&writes==before);
 assert(replace(1,33000,changed,(void *)1)==WV64_CANCELLED&&s.ready);
 verify(1,1041);
 memcpy(saved,durable,sizeof saved);
 before=writes;uint64_t before_flush=flushes;
 assert(replace(1,1501,changed,0)==WV64_OK&&s.sequence==4&&s.inode_epoch==4);
 uint64_t operations=writes-before;
 assert(operations>1&&flushes-before_flush==operations);
 verify(2,1501);
 reboot(false);assert(open_volume(false)==WV64_OK);verify(2,1501);
 assert(replace(2,1,changed,0)==WV64_LOCKED);
 reboot(true);assert(open_volume(true)==WV64_OK);
 assert(replace(2,0,0,0)==WV64_OK&&s.sequence==5);
 verify(3,0);
 reboot(true);assert(open_volume(true)==WV64_OK);verify(3,0);
 assert(replace(3,1501,changed,0)==WV64_OK&&s.sequence==6);verify(4,1501);
 /* Cut every sector write and every flush, including uncertain durable
  * flush, then independently reopen. Only final persisted acknowledgment
  * may expose the new revision. No partially written byte is visible. */
 for(unsigned mode=0;mode<3;mode++)for(uint64_t cut=1;cut<=operations;cut++){
  memcpy(durable,saved,sizeof saved);reboot(true);assert(open_volume(true)==WV64_OK);
  if(!mode)fail_write=writes+cut;else{fail_flush=flushes+cut;persist_failed=mode==2;}
  assert(replace(1,1501,changed,0)==WV64_RECOVERY&&!s.ready);
  reboot(false);assert(open_volume(false)==WV64_OK);
  bool committed=mode==2&&cut==operations;
  verify(committed?2:1,committed?1501:1041);
  reboot(true);
  enum wv64_status rc=open_volume(true);
  if(rc==WV64_RECOVERY)assert(wv64_reconcile(&s,&b,&l,nodes,100,&w,inv)==WV64_OK);
  else assert(rc==WV64_OK);
  verify(committed?2:1,committed?1501:1041);
 }
 printf("STYLEFS 46 ATOMIC SAVE PASS: revisions, empty file, cancellation, independent file, %llu interruption cuts\n",(unsigned long long)(operations*3));
 return 0;
}
