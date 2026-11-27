
#include <styleos/stylefs_pair64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {D=2+16*8,F=23,N=2+D+F};
static uint8_t disk[N][512],durable[N][512],saved[N][512];
static struct bs_service b;
static struct sp64_layout l;
static struct sp64_workspace w;
static struct sp64_put_workspace putwork;
static struct sp64_edit_workspace editwork;
static struct dc64_node nodes[32];
static struct sp64_pair pair;
static unsigned writes,fail;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N)return false;memcpy(p,disk[n],512);return true;}
static bool wr(void *c,uint64_t n,const uint8_t *p){(void)c;assert(n<N);if(++writes==fail)return false;memcpy(disk[n],p,512);return true;}
static bool fl(void *c){(void)c;memcpy(durable,disk,sizeof disk);return true;}
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
 memcpy(disk,durable,sizeof disk);writes=fail=0;attach();
 assert(sp64_mount(&b,&l,&w,nodes,32,&pair)==SP64_OK);
}
static struct sd64_key key(uint64_t parent,const char *name){
 struct sd64_key k={.parent=parent,.length=(uint16_t)strlen(name)};
 assert(k.length<=SD64_NAME);memcpy(k.name,name,k.length);return k;
}
static struct dc64_edit op(enum dc64_edit_op type,uint64_t parent,const char *name,uint64_t id,uint8_t kind){
 return (struct dc64_edit){.op=type,.entry={key(parent,name),id,kind}};
}
static enum sp64_status edit(struct dc64_edit e,uint64_t rev){
 return sp64_edit(&b,&l,&editwork,nodes,32,pair.sequence,&e,rev,&pair);
}
static bool fill(void *c,uint64_t off,uint8_t *out,size_t n){(void)off;memset(out,*(uint8_t *)c,n);return true;}
static enum sp64_status put_file(uint64_t parent,const char *name,uint64_t id,uint8_t value){
 struct sd64_key k=key(parent,name);
 return sp64_put_file(&b,&l,&putwork,nodes,32,&k,id,0,513,fill,&value,&pair);
}
static void check(uint64_t parent,const char *name,uint64_t id,uint8_t value){
 struct sd64_key k=key(parent,name);uint64_t found=0;uint8_t kind=0,p[513];size_t n=0;
 assert(sd64_lookup(&b,7,l.dir,&w.dir.root,&k,w.dw.page,&found,&kind)==SD64_OK&&found==id&&kind==SD64_FILE);
 assert(sfs64_stream_read(&w.data,id,1,0,p,sizeof p,&n)==BS_OK&&n==513);
 for(size_t i=0;i<n;i++)assert(p[i]==value);
}
int main(void){
 attach();assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(sfs64_journal_provision(&b,7,l.data,F,19)==SJ_OK);
 assert(sp64_provision(&b,&l,&w,nodes,32,&pair)==SP64_OK);reopen();
 assert(edit(op(DC64_CREATE,1,"A",2,SD64_DIRECTORY),0)==SP64_OK);
 assert(put_file(2,"file",3,41)==SP64_OK);reopen();check(2,"file",3,41);
 unsigned before=writes;
 struct dc64_edit e=op(DC64_REMOVE,1,"A",2,SD64_DIRECTORY);
 assert(edit(e,0)==SP64_INVALID&&writes==before);
 e=op(DC64_RENAME,2,"file",3,SD64_FILE);e.destination=key(1,"moved");
 assert(sp64_edit(&b,&l,&editwork,nodes,32,pair.sequence-1,&e,1,&pair)==SP64_INVALID&&writes==before);
 assert(edit(e,2)==SP64_INVALID&&writes==before);
 assert(edit(e,1)==SP64_OK);reopen();check(1,"moved",3,41);
 assert(edit(op(DC64_REMOVE,1,"A",2,SD64_DIRECTORY),0)==SP64_OK);reopen();
 check(1,"moved",3,41);
 e=op(DC64_REMOVE,1,"moved",3,SD64_FILE);
 memcpy(saved,durable,sizeof saved);reopen();uint64_t old=pair.sequence;
 assert(edit(e,1)==SP64_OK);unsigned operations=writes;assert(operations==12);
 reopen();assert(pair.sequence==old+1&&!w.dir.count&&!w.data.length[w.data.active]);
 for(unsigned i=1;i<=operations;i++){
  memcpy(durable,saved,sizeof durable);reopen();fail=i;
  assert(edit(e,1)==SP64_UNCERTAIN);
  reopen();assert(pair.sequence==old);check(1,"moved",3,41);
  assert(edit(e,1)==SP64_OK);reopen();assert(!w.dir.count&&!w.data.length[w.data.active]);
 }
 /* Deleted data are protected by the previous bank. With only four
  * sectors total, the next two-sector file must use the other half.
  * Its commit retires the deleted manifest; a third file can reuse the
  * first half without disturbing the second. */
 assert(put_file(1,"second",4,52)==SP64_OK);reopen();check(1,"second",4,52);
 assert(put_file(1,"third",5,63)==SP64_OK);reopen();
 check(1,"second",4,52);check(1,"third",5,63);
 assert(put_file(1,"full",6,74)==SP64_FULL);
 reopen();check(1,"second",4,52);check(1,"third",5,63);
 puts("STYLEFS 27 GUARDED EDIT PASS: mkdir, move, rmdir, revision/sequence guards, 12 delete cuts, retained extents and reuse");
 return 0;
}
