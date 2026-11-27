
#include <styleos/stylefs_pair64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {D=2+16*8,F=256,N=2+D+F};
static uint8_t disk[N][512],durable[N][512],saved[N][512];
static struct bs_service b;
static struct sp64_layout l;
static struct sp64_workspace w,stage;
static struct sp64_put_workspace putwork;
static struct dc64_node nodes[32];
static struct sfs64_stream stream;
static struct sp64_pair pair;
static bool fail_receipt,fail_flush,persist_failed,last_receipt;
static unsigned writes,fail_any;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N)return false;memcpy(p,disk[n],512);return true;}
static bool wr(void *c,uint64_t n,const uint8_t *p){(void)c;assert(n<N);if(++writes==fail_any)return false;last_receipt=n<2;if(fail_receipt&&last_receipt)return false;memcpy(disk[n],p,512);return true;}
static bool fl(void *c){(void)c;if(fail_flush&&last_receipt){if(persist_failed)memcpy(durable,disk,sizeof disk);return false;}memcpy(durable,disk,sizeof disk);return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t g;
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&g)==BS_OK);
 l=(struct sp64_layout){.owner=7,.dir_sectors=D,.data_sectors=F,.data_start=19};
 assert(bs_grant_range(&b,g,7,0,2,3,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,g,7,2,D,3,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D,F,3,&l.data)==BS_OK);
}
static void reopen(void){
 memcpy(disk,durable,sizeof disk);fail_receipt=fail_flush=persist_failed=last_receipt=false;writes=fail_any=0;attach();
 assert(sp64_mount(&b,&l,&w,nodes,32,&pair)==SP64_OK);
}
static struct sd64_key key(void){struct sd64_key k={.parent=1,.length=4,.name={'f','i','l','e'}};return k;}
static void stage_data(uint64_t rev,uint8_t value){
 assert(sfs64_journal_mount(&stage.data,&b,7,l.data,F,19)==SJ_OK);
 assert(sfs64_stream_begin(&stream,&stage.data,2,rev,513)==SJ_OK);
 uint8_t p[513];memset(p,value,sizeof p);
 assert(sfs64_stream_append(&stream,p,sizeof p)==SJ_OK&&sfs64_stream_finish(&stream)==SJ_OK);
}
static void stage_name(void){
 assert(dc64_mount(&stage.dir,&b,7,l.dir,D,&stage.dw.publish)==DC64_OK);
 struct dc64_edit e={.op=DC64_CREATE,.entry={key(),2,SD64_FILE}};
 assert(dc64_hierarchy_apply(&stage.dir,&stage.dw,nodes,32,&e)==DC64_OK);
}
static void check(uint64_t sequence,uint64_t rev,uint8_t value){
 assert(pair.sequence==sequence);
 if(!rev){assert(!w.dir.count);return;}
 assert(w.dir.count==1);
 struct sd64_key k=key();uint64_t id=0;uint8_t kind=0;
 assert(sd64_lookup(&b,7,l.dir,&w.dir.root,&k,w.dw.page,&id,&kind)==SD64_OK&&id==2&&kind==SD64_FILE);
 uint8_t p[513];size_t n=0;
 assert(sfs64_stream_read(&w.data,2,rev,0,p,sizeof p,&n)==BS_OK&&n==513);
 for(size_t i=0;i<n;i++)assert(p[i]==value);
}
static bool fill(void *ctx,uint64_t offset,uint8_t *out,size_t n){
 (void)offset;memset(out,*(uint8_t *)ctx,n);return true;
}
static bool cancel_source(void *ctx,uint64_t offset,uint8_t *out,size_t n){
 (void)ctx;(void)offset;(void)out;(void)n;return false;
}
int main(void){
 attach();
 assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(sfs64_journal_provision(&b,7,l.data,F,19)==SJ_OK);
 assert(sp64_mount(&b,&l,&w,nodes,32,&pair)==SP64_CORRUPT);
 assert(sp64_provision(&b,&l,&w,nodes,32,&pair)==SP64_OK);
 reopen();check(1,0,0);
 stage_data(0,42);
 assert(sp64_commit(&b,&l,&w,nodes,32,1,1,2,&pair)==SP64_CORRUPT);
 reopen();check(1,0,0);
 stage_name();reopen();check(1,0,0);
 memcpy(saved,durable,sizeof saved);
 for(unsigned mode=0;mode<3;mode++){
  memcpy(durable,saved,sizeof durable);reopen();
  fail_receipt=mode==0;fail_flush=mode!=0;persist_failed=mode==2;
  assert(sp64_commit(&b,&l,&w,nodes,32,1,2,2,&pair)==SP64_UNCERTAIN);
  reopen();if(mode==2)check(2,1,42);else check(1,0,0);
 }
 memcpy(durable,saved,sizeof durable);reopen();
 assert(sp64_commit(&b,&l,&w,nodes,32,1,2,2,&pair)==SP64_OK);
 reopen();check(2,1,42);
 stage_data(1,99);reopen();check(2,1,42);
 assert(sp64_commit(&b,&l,&w,nodes,32,2,2,3,&pair)==SP64_OK);
 reopen();check(3,2,99);
 assert(sp64_commit(&b,&l,&w,nodes,32,2,2,3,&pair)==SP64_INVALID);
 /* Corrupt latest receipt: previous pair is still retained. */
 durable[pair.slot][84]^=1;reopen();check(2,1,42);
 /* Recovery after a corrupt newest receipt + abandoned data epoch. */
 assert(sp64_prepare(&b,&l,&w,nodes,32,&pair)==SP64_OK);check(2,1,42);
 stage_data(1,77);reopen();check(2,1,42);
 assert(sp64_prepare(&b,&l,&w,nodes,32,&pair)==SP64_OK);check(2,1,42);
 struct sd64_key k=key();uint8_t value=55;
 assert(sp64_put_file(&b,&l,&putwork,nodes,32,&k,2,1,513,cancel_source,0,&pair)==SP64_CANCELLED);
 reopen();check(2,1,42);
 assert(sp64_put_file(&b,&l,&putwork,nodes,32,&k,2,1,513,fill,&value,&pair)==SP64_OK);
 reopen();check(3,2,55);
 /* Fresh medium; cut every write of guarded create. The next prepare
  * must allow a successful retry without losing the previous pair. */
 memset(disk,0,sizeof disk);memset(durable,0,sizeof durable);attach();
 assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(sfs64_journal_provision(&b,7,l.data,F,19)==SJ_OK);
 assert(sp64_provision(&b,&l,&w,nodes,32,&pair)==SP64_OK);
 memcpy(saved,durable,sizeof saved);reopen();
 assert(sp64_put_file(&b,&l,&putwork,nodes,32,&k,2,0,513,fill,&value,&pair)==SP64_OK);
 unsigned operations=writes;assert(operations==22);
 reopen();check(2,1,55);
 for(unsigned i=1;i<=operations;i++){
  memcpy(durable,saved,sizeof durable);reopen();fail_any=i;
  assert(sp64_put_file(&b,&l,&putwork,nodes,32,&k,2,0,513,fill,&value,&pair)==SP64_UNCERTAIN);
  reopen();check(1,0,0);
  assert(sp64_put_file(&b,&l,&putwork,nodes,32,&k,2,0,513,fill,&value,&pair)==SP64_OK);
  reopen();check(2,1,55);
 }
 /* Both component versions staged; recovery itself interrupted. */
 memcpy(durable,saved,sizeof durable);reopen();stage_data(0,42);stage_name();
 memcpy(saved,durable,sizeof saved);
 for(unsigned i=1;i<=2;i++){
  memcpy(durable,saved,sizeof durable);reopen();fail_any=i;
  assert(sp64_prepare(&b,&l,&w,nodes,32,&pair)==SP64_UNCERTAIN);
  reopen();check(1,0,0);
  assert(sp64_prepare(&b,&l,&w,nodes,32,&pair)==SP64_OK);
  assert(sp64_put_file(&b,&l,&putwork,nodes,32,&k,2,0,513,fill,&value,&pair)==SP64_OK);
  reopen();check(2,1,55);
 }
 puts("STYLEFS 26 RECOVERY PASS: staged discard, guarded put, 22 create cuts, 2 recovery cuts, retry, replacement, cancellation");
 return 0;
}
