#define _POSIX_C_SOURCE 200809L
#include <styleos/stylefs_paged_pair64.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
enum {N=512,D=66,I=66,F=N-2-D-I};
static struct bs_service b;static struct pp64_stream_workspace w;
static struct dc64_node nodes[32];static struct sfs64_run runs[32],previous[32];
static bool source39(void *ctx,uint64_t at,uint8_t *p,size_t n){
 (void)ctx;for(size_t i=0;i<n;i++){uint64_t x=at+i;p[i]=(uint8_t)(x*37u+x/512u*91u+13u);}return true;
}
static const uint8_t text43[]="StyleOS: paged file read test.\nSecond line.\n";
static bool source43(void *ctx,uint64_t at,uint8_t *p,size_t n){
 (void)ctx;if(at>sizeof text43-1||n>sizeof text43-1-at)return false;
 memcpy(p,text43+at,n);return true;
}
static bool source103(void *ctx,uint64_t at,uint8_t *p,size_t n){
 (void)ctx;(void)at;memset(p,'A',n);return true;
}
static bool sink103(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 uint64_t *seen=ctx;if(at!=*seen||at>8193||n>8193-at)return false;
 for(size_t i=0;i<n;++i)if(p[i]!='B')return false;
 *seen+=n;return true;
}
static uint8_t tea69[16384];
static size_t tea69_size;
static bool source69(void *ctx,uint64_t at,uint8_t *p,size_t n){
 (void)ctx;if(at>tea69_size||n>tea69_size-at)return false;
 memcpy(p,tea69+(size_t)at,n);return true;
}
static bool rd(void *c,uint64_t n,uint8_t *p){return n<N&&pread(*(int *)c,p,512,(off_t)(n*512))==512;}
static bool wr(void *c,uint64_t n,const uint8_t *p){return n<N&&pwrite(*(int *)c,p,512,(off_t)(n*512))==512;}
static bool fl(void *c){return fdatasync(*(int *)c)==0;}
static bool sink(void *c,uint64_t at,const uint8_t *p,size_t n){return fwrite(p,1,n,c)==n&&at<1041;}
int main(int argc,char **argv){
 if(argc!=3&&argc!=4)return 1;
 bool ids=!strcmp(argv[1],"new50")||!strcmp(argv[1],"check50");
 bool stream=!strcmp(argv[1],"newtea103");
 bool check_stream=!strcmp(argv[1],"check103");
 bool tea=stream||!strcmp(argv[1],"newtea69");
 bool live=tea||!strcmp(argv[1],"new39")||!strcmp(argv[1],"new50");
 bool create=live||!strcmp(argv[1],"new");
 bool object=!strcmp(argv[1],"get37");
 bool verify=!strcmp(argv[1],"check46")||!strcmp(argv[1],"check50");
 if((create&&argc!=(tea?4:3))||(!create&&(argc!=4||(!verify&&!object&&!check_stream&&strcmp(argv[1],"get")))))return 1;
 if(tea){
  FILE *f=fopen(argv[3],"rb");if(!f)return 1;
  tea69_size=fread(tea69,1,sizeof tea69,f);
  bool valid=!ferror(f)&&fgetc(f)==EOF&&tea69_size>=22;
  if(fclose(f)||!valid)return 1;
 }
 int fd=open(argv[2],create?O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW:O_RDONLY|O_NOFOLLOW,0600);
 if(fd<0)return 1;
 struct stat st;
 bool ok=fstat(fd,&st)==0&&S_ISREG(st.st_mode);
 if(ok&&create)ok=ftruncate(fd,N*512)==0;
 if(ok&&!create)ok=st.st_size==N*512;
 struct bs_backend be={&fd,1,N,create,false,rd,create?wr:0,create?fl:0};uint64_t g;
 struct pp64_layout l={.owner=1,.dir_sectors=D,.inode_sectors=I,.data_sectors=F,.data_start=1};
 unsigned rights=create?3u:1u;
 ok=ok&&bs_attach(&b,&be,&g)==BS_OK&&bs_grant_range(&b,g,1,0,2,rights,&l.receipt)==BS_OK&&
 bs_grant_range(&b,g,1,2,D,rights,&l.dir)==BS_OK&&bs_grant_range(&b,g,1,2+D,I,rights,&l.inode)==BS_OK&&
 bs_grant_range(&b,g,1,2+D+I,F,rights,&l.data)==BS_OK;
 struct pp64_pair pair;
 if(ok&&create)ok=dc64_provision(&b,1,l.dir,D)==DC64_OK&&ir64_provision(&b,1,l.inode,I,F,1)==IP64_OK&&
 (ids?pp64_provision_ids(&b,&l,&w.pair,nodes,32,runs,32,&pair):pp64_provision(&b,&l,&w.pair,nodes,32,runs,32,&pair))==PP64_OK;
 if(ok&&live){
  struct sd64_key k={.parent=1,.length=1,.name={'a'}};
  ok=pp64_put(&b,&l,&w,nodes,32,runs,previous,32,1,&k,2,0,1041,source39,0,&pair)==PP64_OK;
  struct dc64_edit e={.op=DC64_CREATE,.entry={{.parent=1,.length=1,.name={'d'}},3,SD64_DIRECTORY}};
  if(ok)ok=pp64_edit(&b,&l,&w,nodes,32,runs,32,pair.sequence,&e,0,&pair)==PP64_OK;
  k=(struct sd64_key){.parent=3,.length=5,.name={'x','.','t','x','t'}};
  if(ok)ok=pp64_put(&b,&l,&w,nodes,32,runs,previous,32,pair.sequence,&k,4,0,sizeof text43-1,source43,0,&pair)==PP64_OK;
 }
 if(ok&&tea){
  struct dc64_edit e={.op=DC64_CREATE,.entry={{.parent=1,.length=1,.name={'e'}},5,SD64_DIRECTORY}};
  ok=pp64_edit(&b,&l,&w,nodes,32,runs,32,pair.sequence,&e,0,&pair)==PP64_OK;
  struct sd64_key k={.parent=5,.length=15,.name="Native-peer.tea"};
  /* Stream fixture uses a different basename: approval is content-based. */
  if(stream)memcpy(k.name,"Stream-demo.tea",15);
  if(ok)ok=pp64_put(&b,&l,&w,nodes,32,runs,previous,32,pair.sequence,&k,6,0,tea69_size,source69,0,&pair)==PP64_OK;
 }
 if(ok&&stream){
  struct sd64_key k={.parent=3,.length=5,.name="z.bin"};
  ok=pp64_put(&b,&l,&w,nodes,32,runs,previous,32,pair.sequence,&k,7,0,8193,source103,0,&pair)==PP64_OK;
 }
 if(ok&&check_stream){
  uint64_t seen=0;struct sd64_key k={.parent=3,.length=5,.name="z.bin"};
  ok=pp64_mount(&b,&l,&w.pair,nodes,32,runs,32,&pair)==PP64_OK&&
     pp64_get(&b,&l,&w,nodes,32,runs,32,pair.sequence,&k,7,2,sink103,&seen)==PP64_OK&&seen==8193;
 }else if(ok&&verify){
  bool recreated=ids&&!strcmp(argv[3],"recreated");
  bool present=recreated||!strcmp(argv[3],"present");
  ok=(present||!strcmp(argv[3],"absent"))&&pp64_mount(&b,&l,&w.pair,nodes,32,runs,32,&pair)==PP64_OK;
  struct sd64_key k={.parent=3,.length=5,.name={'y','.','t','x','t'}};
  uint64_t id=0;uint8_t kind=0;
  if(ok){
   enum sd64_status rc=sd64_lookup(&b,1,l.dir,&w.pair.dir.root,&k,w.pair.dw.page,&id,&kind);
   if(present){
    struct sfs64_file file;
    ok=pair.sequence==(recreated?9u:6u)&&pair.id_floor==(ids?(recreated?6u:5u):0u)&&rc==SD64_OK&&id==(ids?(recreated?6u:5u):(UINT64_C(0x8000000000000000)|5))&&kind==SD64_FILE&&
     ip64_stat(&b,1,l.inode,&w.pair.inode.root,id,&w.pair.iw,&file)==IP64_OK&&file.bytes==0&&file.revision==1;
   }else ok=pair.sequence==7&&pair.id_floor==(ids?5u:0u)&&rc==SD64_MISSING;
   k.name[0]='z';
   if(ok)ok=sd64_lookup(&b,1,l.dir,&w.pair.dir.root,&k,w.pair.dw.page,&id,&kind)==SD64_MISSING;
  }
 }else if(ok&&!create){
  FILE *out=fopen(argv[3],"wbx");struct sd64_key k={.parent=2,.length=1,.name={'b'}};
  ok=out&&pp64_get(&b,&l,&w,nodes,32,runs,32,object?5:4,&k,3,object?2:1,sink,out)==PP64_OK;
  if(out){if(fclose(out))ok=false;if(!ok)unlink(argv[3]);}
 }
 if(close(fd))ok=false;
 return ok?0:1;
}
