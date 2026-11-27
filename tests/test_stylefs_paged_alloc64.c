#include <styleos/stylefs_paged_pair64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {D=2+16*4,I=2+16*4,F=9,N=2+D+I+F};
static uint8_t disk[N][512],durable[N][512],saved[N][512];
static struct bs_service b;
static struct pp64_layout l;
static struct pp64_workspace w;
static struct dc64_node nodes[32];
static struct sfs64_run current[32],previous[32],plan[4];
static struct pp64_pair pair;
static struct ir64_state inode;
static struct ip64_workspace iw;
static struct dc64_state dir;
static struct dc64_workspace dw;
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
 assert(pp64_mount(&b,&l,&w,nodes,32,current,32,&pair)==PP64_OK);
}
static bool names(void *c,uint64_t i,struct sd64_record *out){
 (void)c;(void)i;struct sd64_key k={.parent=1,.length=1,.name={'f'}};
 *out=(struct sd64_record){k,2,SD64_FILE};return true;
}
struct src{uint64_t rev,first;};
static bool maps(void *c,uint64_t i,uint8_t *p,size_t cap,size_t *n){
 (void)i;struct src *s=c;struct sfs64_file f={2,s->rev,1024,42};
 struct sfs64_run run={s->first,2};*n=sfs64_map_length(1);assert(*n<=cap);
 return sfs64_map_encode(p,*n,f,F,1,&run,1);
}
static void stage_names(void){
 assert(dc64_mount(&dir,&b,7,l.dir,D,&dw)==DC64_OK);
 assert(dc64_publish(&dir,&dw,1,names,0)==DC64_OK);
}
static void stage_map(uint64_t rev,uint64_t first){
 struct src s={rev,first};
 assert(ir64_mount(&inode,&b,7,l.inode,I,F,1,&iw,current,32)==IP64_OK);
 assert(ir64_publish(&inode,&iw,current,32,1,maps,&s)==IP64_OK);
}
static void commit(uint64_t d,uint64_t i){
 assert(pp64_commit(&b,&l,&w,nodes,32,current,32,pair.sequence,d,i,&pair)==PP64_OK);reopen();
}
static enum pp64_status alloc(uint64_t bytes,size_t *n){
 return pp64_plan(&b,&l,&w,nodes,32,current,previous,32,pair.sequence,bytes,plan,4,n);
}
int main(void){
 attach();assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(ir64_provision(&b,7,l.inode,I,F,1)==IP64_OK);
 assert(pp64_provision(&b,&l,&w,nodes,32,current,32,&pair)==PP64_OK);reopen();
 size_t n=0;assert(alloc(1024,&n)==PP64_OK&&n==1&&plan[0].first==1&&plan[0].sectors==2);
 stage_names();stage_map(1,1);commit(2,2);
 assert(alloc(1024,&n)==PP64_OK&&plan[0].first==3);
 stage_map(2,3);reopen();assert(alloc(1024,&n)==PP64_INVALID);
 assert(pp64_prepare(&b,&l,&w,nodes,32,current,32,&pair)==PP64_OK);
 assert(alloc(1024,&n)==PP64_OK&&plan[0].first==3);
 stage_map(2,3);commit(2,3);
 assert(alloc(2048,&n)==PP64_OK&&n==1&&plan[0].first==5&&plan[0].sectors==4);
 assert(alloc(2049,&n)==PP64_FULL);
 stage_map(3,5);commit(2,4);
 assert(alloc(2048,&n)==PP64_OK&&n==2&&plan[0].first==1&&plan[0].sectors==2&&plan[1].first==7);
 /* Abandoned directory and inode stages: interrupt each clear. */
 stage_names();stage_map(4,1);memcpy(saved,durable,sizeof saved);
 for(unsigned cut=1;cut<=2;cut++){
  memcpy(durable,saved,sizeof durable);reopen();fail=cut;
  assert(pp64_prepare(&b,&l,&w,nodes,32,current,32,&pair)==PP64_UNCERTAIN);
  reopen();assert(pair.sequence==4&&pair.dir_epoch==2&&pair.inode_epoch==4);
  assert(pp64_prepare(&b,&l,&w,nodes,32,current,32,&pair)==PP64_OK);
  assert(alloc(1024,&n)==PP64_OK&&plan[0].first==1);
 }
 puts("STYLEFS 31 PAGED ALLOCATION PASS: retained union, fragmented plan, full refusal, abandoned roots, recovery cuts");
 return 0;
}
