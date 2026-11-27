#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include <styleos/stylefs_paged_pair64.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/* Actual sparse regular file. Every sector read/write passes through
 * the block service. Skip only identical writes (including holes already
 * read as zero). Every changed sector is pwrite+fdatasync. Cache is reset
 * on close/reopen; no simulated source bytes are supplied by disk reads. */
#define SIZE ((UINT64_C(100)<<30)+17)
#define N (UINT64_C(220)<<21)
enum {D=66,I=66,W=64};
struct disk {int fd;uint64_t base;bool dirty;uint8_t cache[W*512];};
static struct disk disk;
static struct bs_service b;static struct pp64_layout l;
static struct pp64_stream_workspace w;
static struct dc64_node nodes[32];static struct sfs64_run runs[32],previous[32];
static struct pp64_pair pair;
static bool load(struct disk *d,uint64_t n){
 if(n>=N)return false;
 uint64_t base=n-(n%W);
 if(base==d->base)return true;
 if(pread(d->fd,d->cache,sizeof d->cache,(off_t)(base*512))!=(ssize_t)sizeof d->cache)return false;
 d->base=base;return true;
}
static bool rd(void *ctx,uint64_t n,uint8_t *p){
 struct disk *d=ctx;if(!load(d,n))return false;
 memcpy(p,d->cache+(n-d->base)*512,512);return true;
}
static bool wr(void *ctx,uint64_t n,const uint8_t *p){
 struct disk *d=ctx;if(!load(d,n))return false;
 uint8_t *q=d->cache+(n-d->base)*512;
 if(!memcmp(q,p,512))return true;
 if(pwrite(d->fd,p,512,(off_t)(n*512))!=512)return false;
 memcpy(q,p,512);d->dirty=true;return true;
}
static bool fl(void *ctx){
 struct disk *d=ctx;if(!d->dirty)return true;
 if(fdatasync(d->fd))return false;
 d->dirty=false;return true;
}
static void attach(const char *path,bool create){
 disk.fd=open(path,create?O_CREAT|O_EXCL|O_RDWR|O_NOFOLLOW:O_RDWR|O_NOFOLLOW,0600);
 assert(disk.fd>=0);
 if(create)assert(ftruncate(disk.fd,(off_t)(N*512))==0);
 struct stat st;assert(!fstat(disk.fd,&st)&&S_ISREG(st.st_mode)&&(uint64_t)st.st_size==N*512);
 disk.base=UINT64_MAX;disk.dirty=false;memset(&b,0,sizeof b);
 struct bs_backend be={&disk,1,N,true,false,rd,wr,fl};uint64_t g;
 assert(bs_attach(&b,&be,&g)==BS_OK);
 l=(struct pp64_layout){.owner=7,.dir_sectors=D,.inode_sectors=I,.data_sectors=N-2-D-I,.data_start=1};
 assert(bs_grant_range(&b,g,7,0,2,3,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,g,7,2,D,3,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D,I,3,&l.inode)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D+I,l.data_sectors,3,&l.data)==BS_OK);
}
static void close_disk(void){assert(fl(&disk));assert(!close(disk.fd));}
static void pattern(uint64_t at,uint8_t *p,size_t n){
 memset(p,0,n);
 /* Nonzero address-dependent markers throughout the stream, including
  * every 64 MiB, 4 GiB, and final 17 bytes after 100 GiB. */
 if(!(at% (UINT64_C(64)<<20))){
  size_t m=n<17?n:17;
  for(size_t i=0;i<m;i++)p[i]=(uint8_t)(i<8?((at>>(8*i))&255u):0xA0+i);
 }
}
static bool source(void *c,uint64_t at,uint8_t *p,size_t n){(void)c;pattern(at,p,n);return true;}
static uint8_t expected[SFS64_WINDOW_BYTES];
static bool sink(void *c,uint64_t at,const uint8_t *p,size_t n){
 uint64_t *seen=c;if(at!=*seen||n>sizeof expected)return false;
 pattern(at,expected,n);if(memcmp(p,expected,n))return false;
 *seen+=n;return true;
}
static void mount(void){assert(pp64_mount(&b,&l,&w.pair,nodes,32,runs,32,&pair)==PP64_OK);}
int main(int argc,char **argv){
 assert(argc==2);
 const struct sd64_key a={.parent=1,.length=1,.name={'a'}},z={.parent=1,.length=1,.name={'z'}};
 attach(argv[1],true);
 assert(dc64_provision(&b,7,l.dir,D)==DC64_OK&&ir64_provision(&b,7,l.inode,I,l.data_sectors,1)==IP64_OK);
 assert(pp64_provision_ids(&b,&l,&w.pair,nodes,32,runs,32,&pair)==PP64_OK);
 assert(pp64_put(&b,&l,&w,nodes,32,runs,previous,32,pair.sequence,&a,2,0,SIZE,source,0,&pair)==PP64_OK);
 assert(pair.id_floor==2);close_disk();puts("100GiB+17 import committed");fflush(stdout);
 attach(argv[1],false);mount();
 assert(pp64_copy(&b,&l,&w,nodes,32,runs,previous,32,pair.sequence,&a,2,1,SIZE,&z,3,&pair)==PP64_OK);
 assert(pair.id_floor==3);close_disk();puts("100GiB+17 copy committed");fflush(stdout);
 attach(argv[1],false);mount();assert(pair.id_floor==3);
 uint64_t seen=0;assert(pp64_get(&b,&l,&w,nodes,32,runs,32,pair.sequence,&a,2,1,sink,&seen)==PP64_OK&&seen==SIZE);
 puts("100GiB+17 source verified after reopen");fflush(stdout);
 seen=0;assert(pp64_get(&b,&l,&w,nodes,32,runs,32,pair.sequence,&z,3,1,sink,&seen)==PP64_OK&&seen==SIZE);
 struct stat st;assert(!fstat(disk.fd,&st));
 printf("STYLEFS 50 LARGE SPARSE PASS: %llu bytes each, allocated %llu bytes\n",(unsigned long long)SIZE,(unsigned long long)st.st_blocks*512u);
 close_disk();assert(!unlink(argv[1]));return 0;
}
