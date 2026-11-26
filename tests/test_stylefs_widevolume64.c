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
 assert(wv64_create(&s,&w,inv,&second,0,0,0)==WV64_LOCKED);
 for(size_t i=0;i<n;i++)out[i]=(uint8_t)((off+i)^0x5a);
 return true;
}
static void verify(bool present){
 struct sfs64_file file;enum wv64_status rc=wv64_stat(&s,&w,(const uint8_t *)"/a",2,&file);
 if(!present){assert(rc==WV64_MISSING&&s.sequence==1);return;}
 assert(rc==WV64_OK&&file.id==2&&file.bytes==1041&&s.sequence==2);
 struct sfs64_map m;assert(it64_lookup(&s.inode.table,s.inode_epoch,2,w.stream.old_page,&m)==IT64_OK);
 struct sfs_extent64 e;uint64_t local;size_t n,done;
 assert(sfs64_map_chunk(&m,0,1041,&e,&local,&n)&&n==1041);
 uint8_t out[1041];assert(sfs64_read_window(&b,7,l.data,e,local,out,n,&done)==BS_OK&&done==n);
 for(size_t i=0;i<n;i++)assert(out[i]==(uint8_t)(i^0x5a));
}
int main(void){
 attach(true);
 assert(dc64_provision(&b,7,l.dir,DIR)==DC64_OK);
 assert(ic64_provision(&b,7,l.inode,INO,DATA,32)==IC64_OK);
 assert(open_volume(true)!=WV64_OK);
 assert(wv64_provision_checkpoint(&b,&l,&w)==WV64_OK);
 assert(open_volume(true)==WV64_OK);memcpy(saved,durable,sizeof saved);
 uint64_t before=writes;
 assert(wv64_create(&s,&w,inv,&a,1041,source,(void *)1)==WV64_CANCELLED&&writes==before);
 assert(wv64_create(&s,&w,inv,&a,1041,source,0)==WV64_OK);
 uint64_t operations=writes-before;assert(operations==24);
 reboot(false);assert(open_volume(false)==WV64_OK);verify(true);
 assert(wv64_create(&s,&w,inv,&second,0,0,0)==WV64_LOCKED);
 reboot(true);assert(open_volume(true)==WV64_OK);
 assert(wv64_create(&s,&w,inv,&second,0,0,0)==WV64_OK&&s.sequence==3);
 reboot(true);assert(open_volume(true)==WV64_OK&&s.dir.count==2);
 for(unsigned mode=0;mode<3;mode++)for(uint64_t i=1;i<=operations;i++){
  memcpy(durable,saved,sizeof saved);reboot(true);assert(open_volume(true)==WV64_OK);
  if(!mode)fail_write=writes+i;else{fail_flush=flushes+i;persist_failed=mode==2;}
  assert(wv64_create(&s,&w,inv,&a,1041,source,0)==WV64_RECOVERY&&!s.ready);
  reboot(false);assert(open_volume(false)==WV64_OK);verify(mode==2&&i==operations);
  reboot(true);enum wv64_status rc=open_volume(true);
  if(rc==WV64_OK)assert(s.sequence==(mode==2&&i==operations?2u:1u));
  else assert(!s.ready);
 }
 puts("STYLEFS 34 WIDE VOLUME PASS: checkpointed namespace/inode/data, 72 write/flush cuts, cold readback and write lock");
 return 0;
}
