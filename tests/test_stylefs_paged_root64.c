
#include <styleos/stylefs_paged_root64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {M=2+16*4,D=2+16*8,F=128,N=M+D+F};
static uint8_t disk[N][512],durable[N][512],saved[N][512];
static struct bs_service b;
static struct pr64_layout l;
static struct pr64_workspace w;
static struct dc64_node nodes[64];
static struct sfs64_run runs[64];
static struct pr64_view v;
static unsigned writes,fail,flushes,fail_flush;static bool persist_failed;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N)return false;memcpy(p,disk[n],512);return true;}
static bool wr(void *c,uint64_t n,const uint8_t *p){(void)c;assert(n<N);if(++writes==fail)return false;memcpy(disk[n],p,512);return true;}
static bool fl(void *c){(void)c;if(++flushes==fail_flush){if(persist_failed)memcpy(durable,disk,sizeof disk);return false;}memcpy(durable,disk,sizeof disk);return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t gen;
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK);
 l=(struct pr64_layout){.owner=7,.meta_sectors=M,.dir_sectors=D,.data_sectors=F,.data_start=1};
 assert(bs_grant_range(&b,gen,7,0,M,3,&l.meta)==BS_OK);
 assert(bs_grant_range(&b,gen,7,M,D,3,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,gen,7,M+D,F,3,&l.data)==BS_OK);
}
static void reopen(void){
 memcpy(disk,durable,sizeof disk);writes=fail=flushes=fail_flush=0;persist_failed=false;attach();
 assert(pr64_mount(&b,&l,&w,nodes,64,runs,64,&v)==PR64_OK);
}
static bool dirs(void *c,uint64_t i,struct sd64_record *out){
 (void)c;struct sd64_key k={.parent=1};int n=snprintf((char *)k.name,sizeof k.name,"f%03u",(unsigned)i);
 assert(n>0&&n<SD64_NAME);k.length=(uint16_t)n;
 *out=(struct sd64_record){k,i+2,SD64_FILE};return true;
}
struct source {uint64_t revision;bool bad,cancel;};
static bool maps(void *c,uint64_t i,uint8_t *out,size_t cap,size_t *n){
 struct source *s=c;if(s->cancel&&i==2)return false;
 struct sfs64_file f={i+2+(s->bad?100:0),s->revision,0,0};
 *n=sfs64_map_length(0);assert(*n<=cap);
 return sfs64_map_encode(out,*n,f,F,1,0,0);
}
static void check(uint64_t seq,uint64_t rev){
 assert(v.sequence==seq);
 if(seq==1){assert(!v.inode.files&&!w.dir.count);return;}
 assert(w.dir.count==60&&v.inode.files==60&&v.inode.pages==2);
 for(uint64_t id=2;id<62;id++){
  struct sfs64_file f;
  assert(ip64_stat(&b,7,l.meta,&v.inode,id,&w.iw,&f)==IP64_OK&&f.revision==rev);
 }
}
int main(void){
 attach();assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(pr64_provision(&b,&l,&w,nodes,64,runs,64,&v)==PR64_OK);reopen();check(1,0);
 /* 60 files exceed one inode page; directory must be staged first. */

 assert(dc64_publish(&w.dir,&w.dw.publish,60,dirs,0)==DC64_OK);
 reopen();check(1,0);
 struct source s={1,false,false};memcpy(saved,durable,sizeof saved);
 assert(pr64_publish(&b,&l,&w,nodes,64,runs,64,1,2,60,maps,&s,&v)==PR64_OK);
 unsigned ops=writes;assert(ops==18);reopen();check(2,1);
 for(unsigned mode=0;mode<3;mode++)for(unsigned i=1;i<=ops;i++){
  memcpy(durable,saved,sizeof durable);reopen();
  if(mode==0)fail=i;else{fail_flush=i;persist_failed=mode==2;}
  assert(pr64_publish(&b,&l,&w,nodes,64,runs,64,1,2,60,maps,&s,&v)==PR64_UNCERTAIN);
  reopen();if(mode==2&&i==ops)check(2,1);else check(1,0);
 }
 memcpy(durable,saved,sizeof durable);reopen();
 s.bad=true;assert(pr64_publish(&b,&l,&w,nodes,64,runs,64,1,2,60,maps,&s,&v)==PR64_CORRUPT);
 reopen();check(1,0);s.bad=false;s.cancel=true;
 assert(pr64_publish(&b,&l,&w,nodes,64,runs,64,1,2,60,maps,&s,&v)==PR64_CANCELLED);
 reopen();check(1,0);s.cancel=false;
 assert(pr64_publish(&b,&l,&w,nodes,64,runs,64,1,2,60,maps,&s,&v)==PR64_OK);
 reopen();check(2,1);s.revision=2;
 assert(pr64_publish(&b,&l,&w,nodes,64,runs,64,2,2,60,maps,&s,&v)==PR64_OK);
 reopen();check(3,2);
 durable[v.inode.start][60]^=1;reopen();check(2,1);
 puts("STYLEFS 29 PAGED ROOT PASS: 60 files, paired publication, 54 write/flush cuts, cancellation, orphan refusal, fallback");
 return 0;
}
