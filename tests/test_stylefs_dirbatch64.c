#include <styleos/stylefs_dircommit64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { SECTORS=2+16*64 };
static uint8_t disk[SECTORS][512],saved[SECTORS][512];
static struct bs_service b;static struct dc64_state s;
static struct dc64_workspace w,r;
static uint64_t token,writes,reads,flushes,fail=UINT64_MAX;
static bool rd(void *c,uint64_t l,uint8_t *p){(void)c;++reads;if(l>=SECTORS)return false;memcpy(p,disk[l],512);return true;}
static bool wr(void *c,uint64_t l,const uint8_t *p){(void)c;if(++writes==fail||l>=SECTORS)return false;memcpy(disk[l],p,512);return true;}
static bool fl(void *c){(void)c;++flushes;return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t gen;
 struct bs_backend be={0,1,SECTORS,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK&&bs_grant_range(&b,gen,1,0,SECTORS,BS_READ|BS_WRITE,&token)==BS_OK);
}
static void reopen(void){fail=UINT64_MAX;attach();assert(dc64_mount(&s,&b,1,token,SECTORS,&w)==DC64_OK);}
static struct sd64_key key(unsigned i){
 struct sd64_key k={.parent=1};int n=snprintf((char *)k.name,sizeof k.name,"file-%04u",i);
 assert(n>0&&n<SD64_NAME);k.length=(uint16_t)n;return k;
}
static struct sd64_record item(unsigned i){return (struct sd64_record){key(i),UINT64_C(0x100000000)+i,SD64_FILE};}
static bool next(void *ctx,uint64_t i,struct sd64_record *out){(void)ctx;*out=item((unsigned)i*2);return true;}
static void check_old(void){
 assert(s.count==100);
 for(unsigned i=0;i<100;i++){struct sd64_record x;assert(dc64_entry_at(&s,&r,s.epoch,i,&x)==DC64_OK);struct sd64_record e=item(i*2);assert(x.target==e.target&&sd64_compare(&x.key,&e.key)==0);}
}
static void check_new(void){
 assert(s.count==100);
 unsigned at=0;
 for(unsigned i=0;i<200;i++){
  bool keep=(i%2==0&&i>=16)||(i%2==1&&i<16);
  if(!keep)continue;
  struct sd64_record x,e=item(i);assert(dc64_entry_at(&s,&r,s.epoch,at++,&x)==DC64_OK);
  assert(x.target==e.target&&sd64_compare(&x.key,&e.key)==0);
 }
 assert(at==100);
}
struct cancel_state{unsigned count,limit;};
static bool cancel(void *ctx){struct cancel_state *x=ctx;return ++x->count>=x->limit;}
int main(void){
 attach();assert(dc64_provision(&b,1,token,SECTORS)==DC64_OK);reopen();
 assert(dc64_publish(&s,&w,100,next,0)==DC64_OK);check_old();
 memcpy(saved,disk,sizeof disk);
 struct dc64_edit edits[16]={0};
 for(unsigned i=0;i<8;i++){
  edits[2*i]=(struct dc64_edit){.op=DC64_DELETE,.entry=item(i*2)};
  edits[2*i+1]=(struct dc64_edit){.op=DC64_INSERT,.entry=item(i*2+1)};
 }
 uint64_t start=writes,epoch=s.epoch;
 for(unsigned i=0;i<16;i++)assert(dc64_apply(&s,&w,&r,s.epoch,&edits[i])==DC64_OK);
 uint64_t single=writes-start;check_new();
 memcpy(disk,saved,sizeof disk);reopen();start=writes;
 assert(dc64_apply_batch(&s,&w,&r,epoch,edits,16,0,0)==DC64_OK&&s.epoch==epoch+1);
 uint64_t batch=writes-start;assert(batch<single/10&&batch==74);check_new();
 reopen();check_new();
 memcpy(disk,saved,sizeof disk);reopen();start=writes;
 assert(dc64_apply_batch(&s,&w,&r,epoch-1,edits,16,0,0)==DC64_STALE&&writes==start);
 struct dc64_edit duplicate[2]={edits[1],edits[1]};
 assert(dc64_apply_batch(&s,&w,&r,epoch,duplicate,2,0,0)==DC64_CONFLICT&&writes==start);
 struct cancel_state cs={0,40};
 assert(dc64_apply_batch(&s,&w,&r,epoch,edits,16,cancel,&cs)==DC64_CANCELLED);reopen();check_old();
 for(unsigned i=1;i<=batch;i++){
  memcpy(disk,saved,sizeof disk);reopen();fail=writes+i;
  assert(dc64_apply_batch(&s,&w,&r,epoch,edits,16,0,0)==DC64_UNCERTAIN&&!s.valid);
  reopen();check_old();
 }
 struct dc64_edit moves[2]={{.op=DC64_RENAME,.entry=item(0),.destination=key(201)},
                           {.op=DC64_RENAME,.entry=item(2),.destination=key(1)}};
 assert(dc64_apply_batch(&s,&w,&r,s.epoch,moves,2,0,0)==DC64_OK);
 reopen();struct sd64_record first,last;
 assert(dc64_entry_at(&s,&r,s.epoch,0,&first)==DC64_OK&&first.target==item(2).target);
 assert(dc64_entry_at(&s,&r,s.epoch,99,&last)==DC64_OK&&last.target==item(0).target);
 printf("STYLEFS 24 BATCH PASS: %llu individual vs %llu batch sector writes, cancellation, %llu write failures\n",
 (unsigned long long)single,(unsigned long long)batch,(unsigned long long)batch);
 return 0;
}
