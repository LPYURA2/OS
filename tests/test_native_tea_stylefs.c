#include <styleos/native_tea_stylefs.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {D=130,I=130,F=300,N=2+D+I+F};
static uint8_t disk[N][512],package[NTP_MAX_PACKAGE],bad[NTP_MAX_PACKAGE],scratch[NTP_MAX_PACKAGE];
static uint8_t manifest[4096],hash[32],elfhash[32];
static struct bs_service b;
static struct pp64_layout l;
static struct pp64_stream_workspace w;
static struct dc64_node nodes[128];
static struct sfs64_run runs[128],prior[128];
static struct pp64_pair pair;
static struct po64_service service;
static unsigned writes,used;
static size_t size;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N)return false;memcpy(p,disk[n],512);return true;}
static bool wr(void *c,uint64_t n,const uint8_t *p){(void)c;if(n>=N)return false;++writes;memcpy(disk[n],p,512);return true;}
static bool fl(void *c){(void)c;return true;}
static bool src(void *c,uint64_t at,uint8_t *p,size_t n){
 if(at>size||n>size-at)return false;
 memcpy(p,(const uint8_t *)c+(size_t)at,n);return true;
}
static size_t readfile(const char *name,uint8_t *p,size_t cap){
 FILE *f=fopen(name,"rb");assert(f);size_t n=fread(p,1,cap,f);
 assert(!ferror(f)&&fgetc(f)==EOF);fclose(f);return n;
}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static bool consume(void *c,const uint8_t *p,size_t n){
 ++used;assert(n==size&&!memcmp(p,package,n));return c!=NULL;
}
static void clean(void){for(size_t i=0;i<sizeof scratch;++i)assert(!scratch[i]);}
static struct sd64_key key(char name){struct sd64_key k={.parent=1,.length=1};k.name[0]=(uint8_t)name;return k;}
int main(int argc,char **argv){
 assert(argc==3);size=readfile(argv[1],package,sizeof package);
 size_t m=readfile(argv[2],manifest,sizeof manifest);
 assert(size>30+m&&size<NTP_MAX_PACKAGE);
 size_t at=30+package[26]+((size_t)package[27]<<8)+m;
 assert(at+30<size&&le32(package+at)==0x04034b50u);
 size_t start=at+30+package[at+26]+((size_t)package[at+27]<<8),len=le32(package+at+18);
 assert(start<=size&&len<=size-start);
 nte_sha256(package,size,hash);nte_sha256(package+start,len,elfhash);
 struct nts_pin pin={hash,elfhash,manifest,m,(uint32_t)size};
 memcpy(bad,package,size);bad[size/2]^=1;
 uint64_t g;struct bs_backend be={0,1,N,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&g)==BS_OK);
 l=(struct pp64_layout){.owner=7,.dir_sectors=D,.inode_sectors=I,.data_sectors=F,.data_start=1};
 assert(bs_grant_range(&b,g,7,0,2,3,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,g,7,2,D,3,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D,I,3,&l.inode)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D+I,F,3,&l.data)==BS_OK);
 assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(ir64_provision(&b,7,l.inode,I,F,1)==IP64_OK);
 assert(pp64_provision(&b,&l,&w.pair,nodes,128,runs,128,&pair)==PP64_OK);
 struct sd64_key a=key('a'),z=key('z'),shorter=key('s');
 assert(pp64_put(&b,&l,&w,nodes,128,runs,prior,128,pair.sequence,&a,2,0,size,src,package,&pair)==PP64_OK);
 assert(pp64_put(&b,&l,&w,nodes,128,runs,prior,128,pair.sequence,&z,3,0,size,src,bad,&pair)==PP64_OK);
 assert(pp64_put(&b,&l,&w,nodes,128,runs,prior,128,pair.sequence,&shorter,4,0,size-1,src,package,&pair)==PP64_OK);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,runs,prior,128)==PO64_OK);
 uint64_t token=0,wrong=0,short_token=0;
 assert(po64_grant(&service,10,&a,2,1,&token)==PO64_OK);
 assert(po64_grant(&service,10,&z,3,1,&wrong)==PO64_OK);
 assert(po64_grant(&service,10,&shorter,4,1,&short_token)==PO64_OK);
 unsigned before=writes;
 assert(ntsfs_use(&service,10,token,&pin,scratch,consume,&used)==NTSFS_OK&&used==1);clean();
 assert(ntsfs_use(&service,10,token,&pin,scratch,consume,NULL)==NTSFS_CONSUMER&&used==2);clean();
 assert(ntsfs_use(&service,11,token,&pin,scratch,consume,&used)==NTSFS_READ&&used==2);clean();
 assert(ntsfs_use(&service,10,wrong,&pin,scratch,consume,&used)==NTSFS_PACKAGE&&used==2);clean();
 assert(ntsfs_use(&service,10,short_token,&pin,scratch,consume,&used)==NTSFS_READ&&used==2);clean();
 assert(po64_revoke(&service,10,token)==PO64_OK);
 assert(ntsfs_use(&service,10,token,&pin,scratch,consume,&used)==NTSFS_READ&&used==2);clean();
 assert(writes==before);assert(po64_unmount(&service)==PO64_OK);
 puts("STYLEFS TEA scoped read, integrity, denial and scrub PASS");return 0;
}
