
#include <styleos/stylefs_inode_pages64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {N=8*1800,FILES=100000};
static uint8_t disk[N][512];
static struct bs_service b;
static struct ip64_workspace w;
static uint64_t token,reads,writes,fail_write;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N)return false;reads++;memcpy(p,disk[n],512);return true;}
static bool wr(void *c,uint64_t n,const uint8_t *p){(void)c;assert(n<N);if(++writes==fail_write)return false;memcpy(disk[n],p,512);return true;}
static bool fl(void *c){(void)c;return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t g;
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&g)==BS_OK);
 assert(bs_grant_range(&b,g,7,0,N,BS_READ|BS_WRITE,&token)==BS_OK);
}
struct context {uint64_t count;bool overlap,duplicate;};
static bool next(void *ptr,uint64_t i,uint8_t *out,size_t cap,size_t *n){
 struct context *c=ptr;bool has=i+2>=c->count;
 struct sfs64_file f={UINT64_C(0x100000000)+2*i,1,has?UINT64_C(107374182401):0,has?42u:0u};
 if(c->duplicate&&i==1)f.id-=2;
 struct sfs64_run run={i+1==c->count&&!c->overlap?UINT64_C(500000000):UINT64_C(1000),UINT64_C(209715201)};
 *n=sfs64_map_length(has?1:0);assert(*n<=cap);
 return sfs64_map_encode(out,*n,f,UINT64_C(8589934592),19,&run,has?1:0);
}
int main(void){
 attach();
 struct ip64_root g={.start=0,.sectors=N,.epoch=1,.data_capacity=UINT64_C(8589934592),.data_start=19},root={0};
 struct context c={FILES,false,false};struct sfs64_run runs[2];
 assert(ip64_build(&b,7,token,&g,FILES,next,&c,&w,&root)==IP64_OK);
 assert(root.files==FILES&&root.pages==1695);
 assert(ip64_validate(&b,7,token,&root,&w,runs,2)==IP64_OK);
 assert(ip64_validate(&b,7,token,&root,&w,runs,1)==IP64_FULL);
 for(uint64_t i=0;i<FILES;i+=137){
  struct sfs64_file f={0};reads=0;
  assert(ip64_stat(&b,7,token,&root,UINT64_C(0x100000000)+2*i,&w,&f)==IP64_OK);
  assert(f.id==UINT64_C(0x100000000)+2*i&&reads<=88);
 }
 struct sfs64_file f={0};
 assert(ip64_stat(&b,7,token,&root,UINT64_C(0x100000000)+2*(FILES-1),&w,&f)==IP64_OK&&f.bytes==UINT64_C(107374182401));
 uint64_t id=f.id;assert(ip64_stat(&b,7,token,&root,id-1,&w,&f)==IP64_MISSING&&f.id==id);
 disk[0][63]^=1;assert(ip64_validate(&b,7,token,&root,&w,runs,2)==IP64_CORRUPT);disk[0][63]^=1;
 struct ip64_root bad=root;bad.files++;assert(ip64_validate(&b,7,token,&bad,&w,runs,2)==IP64_CORRUPT);
 c.count=100;c.overlap=true;g.epoch=2;
 assert(ip64_build(&b,7,token,&g,c.count,next,&c,&w,&root)==IP64_OK);
 assert(ip64_validate(&b,7,token,&root,&w,runs,2)==IP64_CORRUPT);
 c.overlap=false;c.duplicate=true;
 assert(ip64_build(&b,7,token,&g,c.count,next,&c,&w,&root)==IP64_INVALID);
 c.duplicate=false;fail_write=writes+1;
 assert(ip64_build(&b,7,token,&g,c.count,next,&c,&w,&root)==IP64_UNCERTAIN);
 attach();fail_write=0;
 assert(ip64_build(&b,7,token,&g,0,0,0,&w,&root)==IP64_OK&&!root.files&&!root.pages);
 assert(ip64_validate(&b,7,token,&root,&w,0,0)==IP64_OK);
 puts("STYLEFS 28 INODE PAGES PASS: 100000 maps, bounded lookup, 100GiB+1 manifest, cross-file overlap and corruption refusal");
 return 0;
}
