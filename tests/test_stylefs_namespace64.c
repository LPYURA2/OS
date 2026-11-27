
#include <styleos/stylefs_dircommit64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {N=2+16*24};
static uint8_t disk[N][512],durable[N][512],saved[N][512];
static struct bs_service b;
static struct dc64_state s;
static struct dc64_edit_workspace w;
static uint64_t token;
static unsigned writes,fail;
static bool rd(void *c,uint64_t l,uint8_t *p){(void)c;if(l>=N)return false;memcpy(p,disk[l],512);return true;}
static bool wr(void *c,uint64_t l,const uint8_t *p){(void)c;assert(l<N);if(++writes==fail)return false;memcpy(disk[l],p,512);return true;}
static bool fl(void *c){(void)c;memcpy(durable,disk,sizeof disk);return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t gen;
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK&&bs_grant_range(&b,gen,7,0,N,BS_READ|BS_WRITE,&token)==BS_OK);
}
static void reopen(void){memcpy(disk,durable,sizeof disk);writes=fail=0;attach();assert(dc64_mount(&s,&b,7,token,N,&w.publish)==DC64_OK);}
static struct sd64_key key(unsigned i){
 struct sd64_key k={.parent=1};int n=snprintf((char *)k.name,sizeof k.name,"entry-%04u",i);assert(n>0&&n<SD64_NAME);k.length=(uint16_t)n;return k;
}
static struct sd64_record rec(unsigned i){return (struct sd64_record){key(i),UINT64_C(0x100000000)+i,SD64_FILE};}
static bool next(void *c,uint64_t i,struct sd64_record *out){(void)c;*out=rec((unsigned)i*2);return true;}
static bool exists(unsigned i,uint64_t target){
 struct sd64_key k=key(i);uint64_t o=0;uint8_t kind=0;
 enum sd64_status rc=s.count?sd64_lookup(&b,7,token,&s.root,&k,w.page,&o,&kind):SD64_MISSING;
 assert(rc==SD64_OK||rc==SD64_MISSING);
 if(rc==SD64_OK)assert(o==target&&kind==SD64_FILE);
 return rc==SD64_OK;
}
static void sorted(void){
 struct sd64_record last={0},cur;
 for(uint64_t i=0;i<s.count;i++){
  assert(dc64_entry_at(&s,i,&w,&cur)==DC64_OK);
  if(i)assert(sd64_compare(&last.key,&cur.key)<0);
  last=cur;
 }
 assert(dc64_entry_at(&s,s.count,&w,&cur)==DC64_INVALID);
}
int main(void){
 attach();assert(dc64_provision(&b,7,token,N)==DC64_OK);reopen();
 assert(dc64_publish(&s,&w.publish,80,next,0)==DC64_OK);sorted();
 struct dc64_state stale=s;
 struct dc64_edit e={.op=DC64_CREATE,.entry=rec(51)};
 assert(dc64_apply(&s,&w,&e)==DC64_OK&&s.count==81&&exists(51,e.entry.target));sorted();
 unsigned before=writes;
 assert(dc64_publish(&stale,&w.publish,0,0,0)==DC64_INVALID&&!stale.valid&&writes==before);
 assert(dc64_apply(&s,&w,&e)==DC64_INVALID&&writes==before);
 e.op=DC64_RENAME;e.destination=key(52);
 assert(dc64_apply(&s,&w,&e)==DC64_INVALID&&writes==before);
 e.destination=key(201);
 memcpy(saved,durable,sizeof saved);reopen();
 assert(dc64_apply(&s,&w,&e)==DC64_OK&&s.count==81&&!exists(51,e.entry.target)&&exists(201,e.entry.target));
 unsigned operations=writes;assert(operations==58);sorted();reopen();sorted();
 for(unsigned i=1;i<=operations;i++){
  memcpy(durable,saved,sizeof durable);reopen();fail=i;
  assert(dc64_apply(&s,&w,&e)==DC64_UNCERTAIN&&!s.valid);
  reopen();assert(s.count==81&&exists(51,e.entry.target)&&!exists(201,e.entry.target));
 }
 memcpy(durable,saved,sizeof durable);reopen();
 e.destination=key(1);assert(dc64_apply(&s,&w,&e)==DC64_OK&&exists(1,e.entry.target)&&!exists(51,e.entry.target));
 sorted();e.entry.key=key(1);e.op=DC64_REMOVE;e.entry.target++;
 before=writes;assert(dc64_apply(&s,&w,&e)==DC64_INVALID&&writes==before);
 e.entry.target--;assert(dc64_apply(&s,&w,&e)==DC64_OK&&s.count==80&&!exists(1,e.entry.target));
 e.entry=rec(158);assert(dc64_apply(&s,&w,&e)==DC64_OK&&s.count==79&&!exists(158,e.entry.target));
 sorted();reopen();sorted();
 e.entry=rec(999);e.entry.kind=SD64_DIRECTORY;e.op=DC64_CREATE;assert(dc64_apply(&s,&w,&e)==DC64_OK);
 e.op=DC64_REMOVE;before=writes;assert(dc64_apply(&s,&w,&e)==DC64_INVALID&&writes==before);
 assert(dc64_publish(&s,&w.publish,0,0,0)==DC64_OK);e.entry=rec(9);e.op=DC64_CREATE;
 assert(dc64_apply(&s,&w,&e)==DC64_OK&&s.count==1);
 e.op=DC64_REMOVE;assert(dc64_apply(&s,&w,&e)==DC64_OK&&!s.count);reopen();assert(!s.count);
 puts("STYLEFS 23 NAMESPACE PASS: create/remove/rename, sorted enumeration, stale writer refusal, 58 interrupted rename writes");
 return 0;
}
