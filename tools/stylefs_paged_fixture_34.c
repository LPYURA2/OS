#define _POSIX_C_SOURCE 200809L
#include <styleos/stylefs_paged_pair64.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
enum {N=512,D=66,I=66,F=N-2-D-I};
static struct bs_service b;static struct pp64_stream_workspace w;
static struct dc64_node nodes[32];static struct sfs64_run runs[32];
static bool rd(void *c,uint64_t n,uint8_t *p){return n<N&&pread(*(int *)c,p,512,(off_t)(n*512))==512;}
static bool wr(void *c,uint64_t n,const uint8_t *p){return n<N&&pwrite(*(int *)c,p,512,(off_t)(n*512))==512;}
static bool fl(void *c){return fdatasync(*(int *)c)==0;}
static bool sink(void *c,uint64_t at,const uint8_t *p,size_t n){return fwrite(p,1,n,c)==n&&at<1041;}
int main(int argc,char **argv){
 if(argc!=3&&argc!=4)return 1;
 bool create=!strcmp(argv[1],"new");
 bool object=!strcmp(argv[1],"get37");
 if((create&&argc!=3)||(!create&&(argc!=4||(!object&&strcmp(argv[1],"get")))))return 1;
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
 pp64_provision(&b,&l,&w.pair,nodes,32,runs,32,&pair)==PP64_OK;
 if(ok&&!create){
  FILE *out=fopen(argv[3],"wbx");struct sd64_key k={.parent=2,.length=1,.name={'b'}};
  ok=out&&pp64_get(&b,&l,&w,nodes,32,runs,32,object?5:4,&k,3,object?2:1,sink,out)==PP64_OK;
  if(out){if(fclose(out))ok=false;if(!ok)unlink(argv[3]);}
 }
 if(close(fd))ok=false;
 return ok?0:1;
}
