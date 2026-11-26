#include <styleos/stylefs_checkpoint64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { DIR=258,DATA=4096,TOTAL=2+DIR+DATA };
static uint8_t disk[TOTAL][512],saved[TOTAL][512];
static struct bs_service b;
static struct dc64_state d;
static struct sfs64_journal j;
static struct ns64_proof p;
static struct ns64_node nodes[100];
static struct fb64_view v;
static struct fb64_workspace w;
static struct cp64_layout l;
static struct cp64_result result;
static uint64_t gen,writes,fail=UINT64_MAX;
static bool rd(void *ctx,uint64_t a,uint8_t *out){(void)ctx;if(a>=TOTAL)return false;memcpy(out,disk[a],512);return true;}
static bool wr(void *ctx,uint64_t a,const uint8_t *in){(void)ctx;if(++writes==fail||a>=TOTAL)return false;memcpy(disk[a],in,512);return true;}
static bool fl(void *ctx){(void)ctx;return true;}
static void attach(bool write){
 struct bs_backend be={0,1,TOTAL,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK);
 unsigned rights=BS_READ|(write?BS_WRITE:0);
 l=(struct cp64_layout){.owner=7,.dir_sectors=DIR,.data_sectors=DATA,.data_start=32};
 assert(bs_grant_range(&b,gen,7,0,2,rights,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2,DIR,rights,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2+DIR,DATA,rights,&l.data)==BS_OK);
}
static void reopen(bool write){assert(bs_detach(&b,gen)==BS_OK);fail=UINT64_MAX;attach(write);}
static void mount(void){
 assert(dc64_mount(&d,&b,7,l.dir,DIR,&w.writer)==DC64_OK);
 assert(sfs64_journal_mount(&j,&b,7,l.data,DATA,32)==SJ_OK);
 assert(ns64_validate(&d,&w.reader,nodes,100,&p)==NS64_OK);
 assert(fb64_bind(&v,&d,&p,&j)==FB64_OK);
}
static void recover(uint64_t seq,bool fallback){
 uint64_t before=writes;
 assert(cp64_recover(&b,&l,&d,&j,&p,nodes,100,&w.reader,&v,&result)==CP64_OK);
 assert(result.sequence==seq&&result.fallback==fallback&&writes==before);
 assert(cp64_save(&b,&l,&v)==CP64_STALE);
}
static bool source(void *ctx,uint64_t off,uint8_t *buf,size_t n){
 (void)ctx;for(size_t i=0;i<n;i++)buf[i]=(uint8_t)((off+i)^0x55);return true;
}
static struct sd64_record entry={.key={.parent=1,.length=4,.name={'f','i','l','e'}},.target=2,.kind=SD64_FILE};
int main(void){
 attach(true);
 assert(dc64_provision(&b,7,l.dir,DIR)==DC64_OK);
 assert(sfs64_journal_provision(&b,7,l.data,DATA,32)==SJ_OK);
 mount();assert(cp64_save(&b,&l,&v)==CP64_OK);
 assert(fb64_create(&v,&w,&entry,1041,source,0)==FB64_OK);
 mount();memcpy(saved,disk,sizeof saved);
 /* Uncheckpointed valid pair is NOT selected by recovery. */
 reopen(false);recover(1,false);assert(d.count==0);
 assert(fb64_create(&v,&w,&entry,1041,source,0)==FB64_STALE);
 reopen(true);mount();assert(d.count==1);
 fail=writes+1;assert(cp64_save(&b,&l,&v)==CP64_UNCERTAIN);
 reopen(false);recover(1,false);assert(d.count==0);
 memcpy(disk,saved,sizeof disk);reopen(true);mount();assert(cp64_save(&b,&l,&v)==CP64_OK);
 reopen(false);recover(2,false);assert(d.count==1);
 struct sfs64_file file;
 assert(fb64_stat(&v,(const uint8_t *)"/file",5,w.reader.page,&file)==FB64_OK&&file.bytes==1041);
 /* Newer data bank header lost; exact older receipt remains usable. */
 disk[2+DIR+10][0]^=1;
 reopen(false);recover(1,true);assert(d.count==0);
 puts("STYLEFS 27 CHECKPOINT PASS: exact pair, uncheckpointed exclusion, receipt failure, corruption fallback, read-only recovery");
 return 0;
}
