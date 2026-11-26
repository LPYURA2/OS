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


static bool progress(void *ctx,uint64_t done,uint64_t total){
 assert(done<=total&&wv64_create(&s,&w,inv,&second,0,0,0)==WV64_LOCKED);
 return !ctx;
}
static uint8_t data[70001],check[32768];
static struct sd64_record folder={.key={.parent=1,.length=1,.name={'d'}},.target=10,.kind=SD64_DIRECTORY};
int main(void){
 attach(true);
 assert(dc64_provision(&b,7,l.dir,DIR)==DC64_OK);
 assert(ic64_provision(&b,7,l.inode,INO,DATA,32)==IC64_OK);
 assert(wv64_provision_checkpoint(&b,&l,&w)==WV64_OK);
 assert(open_volume(true)==WV64_OK);
 assert(wv64_create(&s,&w,inv,&a,sizeof data,source,0)==WV64_OK);
 for(size_t i=0;i<sizeof data;i++)data[i]=(uint8_t)(i^0x5a);
 struct dc64_edit mk={.op=DC64_INSERT,.entry=folder};
 assert(wv64_edit(&s,&w,inv,&mk)==WV64_OK);
 struct wv64_cursor cursor;struct sd64_record records[SD64_FANOUT];unsigned count;
 assert(wv64_list_open(&s,&w,(const uint8_t *)"/",1,&cursor)==WV64_OK);
 assert(wv64_list_next(&s,&w,&cursor,records,&count)==WV64_OK&&count==2&&records[0].target==2&&records[1].target==10);
 assert(wv64_list_next(&s,&w,&cursor,records,&count)==WV64_OK&&!count);
 size_t done=99;uint64_t before=writes;
 assert(wv64_read(&s,&w,(const uint8_t *)"/a",2,2,1,511,check,sizeof check,&done)==WV64_OK&&done==sizeof check);
 assert(!memcmp(check,data+511,done)&&writes==before);
 assert(wv64_read(&s,&w,(const uint8_t *)"/a",2,2,2,0,check,1,&done)==WV64_CONFLICT);
 assert(wv64_read(&s,&w,(const uint8_t *)"/a",2,2,1,sizeof data-5,check,100,&done)==WV64_OK&&done==5);
 assert(!memcmp(check,data+sizeof data-5,5));
 second.key.parent=10;
 assert(wv64_copy(&s,&w,inv,(const uint8_t *)"/a",2,2,1,&second,progress,(void *)1)==WV64_CANCELLED&&s.ready);
 struct sfs64_file file;
 assert(wv64_stat(&s,&w,(const uint8_t *)"/d/b",4,&file)==WV64_MISSING);
 assert(wv64_copy(&s,&w,inv,(const uint8_t *)"/a",2,2,1,&second,progress,0)==WV64_OK);
 assert(wv64_list_next(&s,&w,&cursor,records,&count)==WV64_LOCKED);
 reboot(false);assert(open_volume(false)==WV64_OK);
 assert(wv64_list_open(&s,&w,(const uint8_t *)"/d",2,&cursor)==WV64_OK);
 assert(wv64_list_next(&s,&w,&cursor,records,&count)==WV64_OK&&count==1&&records[0].target==3);
 assert(wv64_stat(&s,&w,(const uint8_t *)"/d/b",4,&file)==WV64_OK&&file.bytes==sizeof data);
 for(uint64_t off=0;off<sizeof data;){
  assert(wv64_read(&s,&w,(const uint8_t *)"/d/b",4,3,1,off,check,sizeof check,&done)==WV64_OK&&done);
  assert(!memcmp(check,data+off,done));off+=done;
 }
 reboot(true);assert(open_volume(true)==WV64_OK);
 memcpy(saved,durable,sizeof saved);
 struct sfs64_map m;assert(it64_lookup(&s.inode.table,s.inode_epoch,2,w.stream.old_page,&m)==IT64_OK);
 struct sfs_extent64 e;uint64_t local;size_t n;
 assert(sfs64_map_chunk(&m,0,1,&e,&local,&n));
 disk[2+DIR+INO+e.first][0]^=1;
 struct sd64_record third={.key={.parent=1,.length=1,.name={'c'}},.target=4,.kind=SD64_FILE};
 assert(wv64_copy(&s,&w,inv,(const uint8_t *)"/a",2,2,1,&third,0,0)==WV64_CORRUPT&&!s.ready);
 /* Interrupted staged write: original remains, destination is absent. */
 memcpy(durable,saved,sizeof saved);reboot(true);assert(open_volume(true)==WV64_OK);
 fail_write=writes+1;
 assert(wv64_copy(&s,&w,inv,(const uint8_t *)"/a",2,2,1,&third,0,0)==WV64_RECOVERY&&!s.ready);
 reboot(false);assert(open_volume(false)==WV64_OK);
 assert(wv64_stat(&s,&w,(const uint8_t *)"/c",2,&file)==WV64_MISSING);
 assert(wv64_read(&s,&w,(const uint8_t *)"/a",2,2,1,0,check,sizeof check,&done)==WV64_OK&&!memcmp(check,data,done));
 puts("STYLEFS 36 READ COPY PASS: bounded unaligned reads, directory cursor, 70001-byte copy, cancellation, CRC and write failure refusal");
 return 0;
}
