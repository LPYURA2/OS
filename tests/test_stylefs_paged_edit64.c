#include <styleos/stylefs_paged_pair64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {D=2+16*8,I=2+16*8,F=300,N=2+D+I+F};
static uint8_t disk[N][512],durable[N][512],saved[N][512];
static struct bs_service b;static struct pp64_layout l;
static struct pp64_stream_workspace w;
static struct dc64_node nodes[128];
static struct sfs64_run current[128],previous[128];
static struct pp64_pair pair;
static unsigned writes,fail;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N)return false;memcpy(p,disk[n],512);return true;}
static bool wr(void *c,uint64_t n,const uint8_t *p){(void)c;assert(n<N);if(++writes==fail)return false;memcpy(disk[n],p,512);return true;}
static bool fl(void *c){(void)c;memcpy(durable,disk,sizeof disk);return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t g;
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};assert(bs_attach(&b,&be,&g)==BS_OK);
 l=(struct pp64_layout){.owner=7,.dir_sectors=D,.inode_sectors=I,.data_sectors=F,.data_start=1};
 assert(bs_grant_range(&b,g,7,0,2,3,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,g,7,2,D,3,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D,I,3,&l.inode)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D+I,F,3,&l.data)==BS_OK);
}
static void reopen(void){
 memcpy(disk,durable,sizeof disk);writes=fail=0;attach();
 assert(pp64_mount(&b,&l,&w.pair,nodes,128,current,128,&pair)==PP64_OK);
}
static struct sd64_key key(char name){struct sd64_key k={.parent=1,.length=1};k.name[0]=(uint8_t)name;return k;}
struct pattern{uint8_t seed;uint64_t cancel,seen;};
static uint8_t byte(uint8_t seed,uint64_t i){return (uint8_t)(seed+i*17u);}
static bool source(void *ctx,uint64_t at,uint8_t *p,size_t n){
 struct pattern *s=ctx;if(at>=s->cancel)return false;
 for(size_t i=0;i<n;i++)p[i]=byte(s->seed,at+i);
 return true;
}
static bool sink(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 struct pattern *s=ctx;assert(at==s->seen);
 for(size_t i=0;i<n;i++)if(p[i]!=byte(s->seed,at+i))return false;
 s->seen+=n;return true;
}
static enum pp64_status put(char name,uint64_t id,uint64_t rev,uint64_t bytes,struct pattern *s){
 struct sd64_key k=key(name);
 return pp64_put(&b,&l,&w,nodes,128,current,previous,128,pair.sequence,&k,id,rev,bytes,source,s,&pair);
}
static void check(char name,uint64_t id,uint64_t rev,uint64_t bytes,uint8_t seed){
 struct sd64_key k=key(name);struct pattern s={seed,UINT64_MAX,0};
 assert(pp64_get(&b,&l,&w,nodes,128,current,128,pair.sequence,&k,id,rev,sink,&s)==PP64_OK&&s.seen==bytes);
}
static enum pp64_status edit(enum dc64_edit_op op,uint64_t parent,char name,uint64_t id,uint8_t kind,uint64_t rev,uint64_t dst_parent,char dst){
 struct sd64_key k=key(name);k.parent=parent;
 struct dc64_edit e={.op=op,.entry={k,id,kind}};
 e.destination=key(dst);e.destination.parent=dst_parent;
 return pp64_edit(&b,&l,&w,nodes,128,current,128,pair.sequence,&e,rev,&pair);
}
int main(void){
 attach();assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(ir64_provision(&b,7,l.inode,I,F,1)==IP64_OK);
 assert(pp64_provision(&b,&l,&w.pair,nodes,128,current,128,&pair)==PP64_OK);reopen();
 struct pattern s={31,UINT64_MAX,0};
 assert(put('a',2,0,513,&s)==PP64_OK);reopen();check('a',2,1,513,31);
 s.seed=44;assert(put('b',3,0,513,&s)==PP64_OK);reopen();check('b',3,1,513,44);
 assert(edit(DC64_CREATE,1,'d',4,SD64_DIRECTORY,0,0,0)==PP64_OK);reopen();
 assert(edit(DC64_RENAME,1,'a',2,SD64_FILE,1,4,'x')==PP64_OK);reopen();
 unsigned before=writes;
 assert(edit(DC64_REMOVE,1,'d',4,SD64_DIRECTORY,0,0,0)==PP64_INVALID&&writes==before);
 assert(edit(DC64_REMOVE,4,'x',2,SD64_FILE,2,0,0)==PP64_INVALID&&writes==before);
 assert(edit(DC64_RENAME,4,'x',2,SD64_FILE,1,1,'z')==PP64_OK);reopen();
 check('z',2,1,513,31);check('b',3,1,513,44);
 assert(edit(DC64_REMOVE,1,'d',4,SD64_DIRECTORY,0,0,0)==PP64_OK);reopen();
 memcpy(saved,durable,sizeof saved);uint64_t old=pair.sequence;
 assert(edit(DC64_REMOVE,1,'z',2,SD64_FILE,1,0,0)==PP64_OK);
 unsigned ops=writes;assert(ops==21);reopen();assert(pair.sequence==old+1&&w.pair.inode.root.files==1);check('b',3,1,513,44);
 for(unsigned cut=1;cut<=ops;cut++){
  memcpy(durable,saved,sizeof durable);reopen();fail=cut;
  assert(edit(DC64_REMOVE,1,'z',2,SD64_FILE,1,0,0)==PP64_UNCERTAIN);
  reopen();assert(pair.sequence==old);check('z',2,1,513,31);check('b',3,1,513,44);
  assert(edit(DC64_REMOVE,1,'z',2,SD64_FILE,1,0,0)==PP64_OK);reopen();
  assert(w.pair.inode.root.files==1);check('b',3,1,513,44);
 }
 assert(edit(DC64_REMOVE,1,'b',3,SD64_FILE,1,0,0)==PP64_OK);reopen();
 assert(!w.pair.inode.root.files&&!w.pair.dir.count);
 s.seed=55;assert(put('c',5,0,513,&s)==PP64_OK);reopen();check('c',5,1,513,55);
 puts("STYLEFS 33 PAGED EDIT PASS: mkdir, move, rmdir, stale revision, 21 unlink cuts, retry and extent reuse");
 return 0;
}
