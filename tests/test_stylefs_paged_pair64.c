#include <styleos/stylefs_paged_pair64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {D=2+16*8,I=2+16*8,F=256,N=2+D+I+F};
static uint8_t disk[N][512],durable[N][512],saved[N][512];
static struct bs_service b;
static struct pp64_layout l;
static struct pp64_workspace w;
static struct dc64_node nodes[128];
static struct sfs64_run runs[128];
static struct pp64_pair pair;
static struct ir64_state inode;
static struct ip64_workspace iw;
static struct dc64_state dir;
static struct dc64_workspace dw;
static unsigned fail_mode;static bool receipt;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N)return false;memcpy(p,disk[n],512);return true;}
static bool wr(void *c,uint64_t n,const uint8_t *p){(void)c;assert(n<N);receipt=n<2;if(receipt&&fail_mode==1)return false;memcpy(disk[n],p,512);return true;}
static bool fl(void *c){(void)c;if(receipt&&fail_mode==2)return false;memcpy(durable,disk,sizeof disk);return !(receipt&&fail_mode==3);}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t g;
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&g)==BS_OK);
 l=(struct pp64_layout){.owner=7,.dir_sectors=D,.inode_sectors=I,.data_sectors=F,.data_start=1};
 assert(bs_grant_range(&b,g,7,0,2,3,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,g,7,2,D,3,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D,I,3,&l.inode)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D+I,F,3,&l.data)==BS_OK);
}
static void reopen(void){
 memcpy(disk,durable,sizeof disk);fail_mode=0;receipt=false;attach();
 assert(pp64_mount(&b,&l,&w,nodes,128,runs,128,&pair)==PP64_OK);
}
static bool names(void *c,uint64_t i,struct sd64_record *out){
 (void)c;struct sd64_key k={.parent=1};int n=snprintf((char *)k.name,sizeof k.name,"file-%04llu",(unsigned long long)i);
 assert(n>0&&n<SD64_NAME);k.length=(uint16_t)n;*out=(struct sd64_record){k,i+2,SD64_FILE};return true;
}
static bool maps(void *c,uint64_t i,uint8_t *p,size_t cap,size_t *n){
 uint64_t rev=*(uint64_t *)c;struct sfs64_file f={i+2,rev,0,0};
 *n=sfs64_map_length(0);assert(*n<=cap);
 return sfs64_map_encode(p,*n,f,F,1,0,0);
}
static void stage_names(uint64_t n){
 assert(dc64_mount(&dir,&b,7,l.dir,D,&dw)==DC64_OK);
 assert(dc64_publish(&dir,&dw,n,names,0)==DC64_OK);
}
static void stage_maps(uint64_t n,uint64_t rev){
 assert(ir64_mount(&inode,&b,7,l.inode,I,F,1,&iw,runs,128)==IP64_OK);
 assert(ir64_publish(&inode,&iw,runs,128,n,maps,&rev)==IP64_OK);
}
static void check(uint64_t seq,uint64_t count,uint64_t rev){
 assert(pair.sequence==seq&&w.dir.count==count&&w.inode.root.files==count);
 for(uint64_t i=0;i<count;i++){
  struct sfs64_file f;
  assert(ip64_stat(&b,7,l.inode,&w.inode.root,i+2,&w.iw,&f)==IP64_OK&&f.revision==rev);
 }
}
int main(void){
 attach();assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(ir64_provision(&b,7,l.inode,I,F,1)==IP64_OK);
 assert(pp64_provision(&b,&l,&w,nodes,128,runs,128,&pair)==PP64_OK);
 reopen();check(1,0,0);
 stage_maps(80,1);
 assert(pp64_commit(&b,&l,&w,nodes,128,runs,128,1,1,2,&pair)==PP64_CORRUPT);
 stage_names(80);reopen();check(1,0,0);
 memcpy(saved,durable,sizeof saved);
 for(unsigned mode=1;mode<=3;mode++){
  memcpy(durable,saved,sizeof durable);reopen();fail_mode=mode;
  assert(pp64_commit(&b,&l,&w,nodes,128,runs,128,1,2,2,&pair)==PP64_UNCERTAIN);
  reopen();if(mode==3)check(2,80,1);else check(1,0,0);
 }
 memcpy(durable,saved,sizeof durable);reopen();
 assert(pp64_commit(&b,&l,&w,nodes,128,runs,128,1,2,2,&pair)==PP64_OK);
 reopen();check(2,80,1);
 stage_maps(80,2);reopen();check(2,80,1);
 assert(pp64_commit(&b,&l,&w,nodes,128,runs,128,2,2,3,&pair)==PP64_OK);
 reopen();check(3,80,2);
 assert(pp64_mount(&b,&l,&w,nodes,79,runs,128,&pair)==PP64_FULL);
 struct pp64_layout bad=l;bad.data=bad.inode;bad.data_sectors=I;
 assert(pp64_mount(&b,&bad,&w,nodes,128,runs,128,&pair)==PP64_INVALID);
 durable[0][100]^=1;reopen();check(2,80,1);
 puts("STYLEFS 30 PAGED PAIR PASS: 80 manifests beyond old bank, exact epochs, mismatch refusal, three receipt cuts and fallback");
 return 0;
}
