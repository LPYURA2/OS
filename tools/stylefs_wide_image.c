#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include <styleos/stylefs_stream64.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

/* Explicit host-only candidate image tool. File IDs, no namespace/ACL yet.
 * Uses the real integrated stream/journal with durable sector flushes.
 * Only regular files, advisory exclusive writer lock, no device nodes. */
static struct bs_service block;
static struct sfs64_journal journal;
static struct sfs64_stream stream;
static uint8_t buffer[SFS64_WINDOW_BYTES];
struct image {int fd;uint64_t sectors;};
static bool rd(void *ctx,uint64_t lba,uint8_t out[512]){
    struct image *d=ctx;
    return lba<d->sectors&&pread(d->fd,out,512,(off_t)(lba*512))==512;
}
static bool wr(void *ctx,uint64_t lba,const uint8_t in[512]){
    struct image *d=ctx;
    return lba<d->sectors&&pwrite(d->fd,in,512,(off_t)(lba*512))==512;
}
static bool fl(void *ctx){return fdatasync(((struct image *)ctx)->fd)==0;}
static bool regular(int fd,struct stat *s)
{return fd>=0&&fstat(fd,s)==0&&S_ISREG(s->st_mode)&&s->st_size>=0;}
static bool number(const char *p,uint64_t *out){
    if(!p||!*p)return false;
    uint64_t n=0;
    for(;*p;++p){
        if(*p<'0'||*p>'9'||n>(UINT64_MAX-(unsigned)(*p-'0'))/10)return false;
        n=n*10+(unsigned)(*p-'0');
    }
    *out=n;return true;
}
static bool attach(struct image *d,bool write,uint64_t *token){
    const struct bs_backend backend={d,1,d->sectors,write,false,rd,write?wr:0,write?fl:0};
    uint64_t gen;
    return bs_attach(&block,&backend,&gen)==BS_OK&&
           bs_grant_range(&block,gen,1,0,d->sectors,BS_READ|(write?BS_WRITE:0),token)==BS_OK;
}
static int temp(const char *path,char **name){
    const char suffix[]=".wide-XXXXXX";size_t n=strlen(path);
    if(n>SIZE_MAX-sizeof suffix)return -1;
    *name=malloc(n+sizeof suffix);if(!*name)return -1;
    memcpy(*name,path,n);memcpy(*name+n,suffix,sizeof suffix);
    int fd=mkstemp(*name);if(fd<0){free(*name);*name=0;}return fd;
}
static bool sync_parent(const char *path){
    char *s=strdup(path);if(!s)return false;
    char *slash=strrchr(s,'/');
    const char *dir=".";
    if(slash){if(slash==s){slash[1]=0;}else *slash=0;dir=s;}
    int fd=open(dir,O_RDONLY|O_DIRECTORY);bool ok=fd>=0&&fsync(fd)==0;
    if(fd>=0)close(fd);
    free(s);return ok;
}
static bool publish(int fd,char *name,const char *path,bool ok){
    if(ok&&fsync(fd)!=0)ok=false;
    if(close(fd)!=0)ok=false;
    bool linked=false;
    if(ok){linked=link(name,path)==0;ok=linked;}
    if(unlink(name)!=0)ok=false;
    free(name);
    /* If directory fsync fails after link, the result may exist; report
     * failure without retrying or deleting either user's original file. */
    if(linked&&!sync_parent(path))ok=false;
    return ok;
}
static uint32_t crc(uint32_t c,const uint8_t *p,size_t n){
    for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
    return c;
}
static bool create(const char *path,uint64_t sectors,uint64_t start){
    if(sectors>(uint64_t)INT64_MAX/512||start<SFS64_JOURNAL_SECTORS||start>=sectors)return false;
    char *name=0;struct image d={temp(path,&name),sectors};
    if(d.fd<0)return false;
    uint64_t token;
    bool ok=ftruncate(d.fd,(off_t)(sectors*512))==0&&attach(&d,true,&token)&&
        sfs64_journal_provision(&block,1,token,sectors,start)==SJ_OK&&
        sfs64_journal_mount(&journal,&block,1,token,sectors,start)==SJ_OK&&
        journal.generation[journal.active]==1&&journal.length[journal.active]==0;
    return publish(d.fd,name,path,ok);
}
static bool input(const char *path,struct image *d,uint64_t id,uint64_t rev){
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    struct stat st,img,after;
    bool ok=regular(fd,&st)&&fstat(d->fd,&img)==0&&
        (st.st_dev!=img.st_dev||st.st_ino!=img.st_ino)&&
        flock(fd,LOCK_SH|LOCK_NB)==0;
    if(ok)ok=sfs64_stream_begin(&stream,&journal,id,rev,(uint64_t)st.st_size)==SJ_OK;
    uint64_t size=ok?(uint64_t)st.st_size:0;
    for(uint64_t off=0;ok&&off<size;){
        size_t n=size-off<sizeof buffer?(size_t)(size-off):sizeof buffer;
        ok=pread(fd,buffer,n,(off_t)off)==(ssize_t)n&&sfs64_stream_append(&stream,buffer,n)==SJ_OK;
        off+=n;
    }
    if(ok)ok=fstat(fd,&after)==0&&st.st_size==after.st_size&&
        st.st_mtim.tv_sec==after.st_mtim.tv_sec&&st.st_mtim.tv_nsec==after.st_mtim.tv_nsec&&
        st.st_ctim.tv_sec==after.st_ctim.tv_sec&&st.st_ctim.tv_nsec==after.st_ctim.tv_nsec;
    if(ok)ok=sfs64_stream_finish(&stream)==SJ_OK;
    if(!ok)sfs64_stream_cancel(&stream);
    if(fd>=0)close(fd);
    return ok;
}
static bool output(const char *path,uint64_t id){
    struct sfs64_file file;
    if(sfs64_stream_stat(&journal,id,&file)!=SJ_OK||file.bytes>INT64_MAX)return false;
    char *name=0;int fd=temp(path,&name);if(fd<0)return false;
    bool ok=true;uint32_t c=UINT32_MAX;
    for(uint64_t off=0;ok&&off<file.bytes;){
        size_t n=file.bytes-off<sizeof buffer?(size_t)(file.bytes-off):sizeof buffer,done=0;
        ok=sfs64_stream_read(&journal,id,file.revision,off,buffer,n,&done)==BS_OK&&done&&done<=n;
        if(ok){c=crc(c,buffer,done);ok=pwrite(fd,buffer,done,(off_t)off)==(ssize_t)done;off+=done;}
    }
    return publish(fd,name,path,ok&&~c==file.checksum);
}
int main(int argc,char **argv){
    if(argc<2)goto usage;
    const char *cmd=argv[1];
    uint64_t cap,start,id,rev,token;
    bool ok=false;
    if(!strcmp(cmd,"new")&&argc==5){
        ok=number(argv[3],&cap)&&number(argv[4],&start)&&create(argv[2],cap,start);
    }else if((!strcmp(cmd,"put")&&argc==7)||(!strcmp(cmd,"get")&&argc==6)||
             (!strcmp(cmd,"stat")&&argc==5)){
        if(!number(argv[3],&start)||!number(argv[4],&id)||!id)goto usage;
        bool write=!strcmp(cmd,"put");
        if(write&&!number(argv[5],&rev))goto usage;
        struct image d={open(argv[2],(write?O_RDWR:O_RDONLY)|O_NOFOLLOW|O_NONBLOCK),0};
        struct stat st;
        if(regular(d.fd,&st)&&st.st_size>0&&st.st_size%512==0&&
           flock(d.fd,(write?LOCK_EX:LOCK_SH)|LOCK_NB)==0){
            d.sectors=(uint64_t)st.st_size/512;
            if(attach(&d,write,&token)&&sfs64_journal_mount(&journal,&block,1,token,d.sectors,start)==SJ_OK){
                if(write)ok=input(argv[6],&d,id,rev);
                else if(!strcmp(cmd,"get"))ok=output(argv[5],id);
                else {struct sfs64_file file;
                    if(sfs64_stream_stat(&journal,id,&file)==SJ_OK){
                        printf("id=%" PRIu64 " revision=%" PRIu64 " bytes=%" PRIu64 " crc=%08" PRIx32 "\n",
                               file.id,file.revision,file.bytes,file.checksum);
                        ok=!ferror(stdout);
                    }
                }
            }
        }
        if(d.fd>=0&&close(d.fd)!=0)ok=false;
    }else goto usage;
    if(!ok)fputs("wide image operation refused or uncertain; inspect/remount, never blindly retry\n",stderr);
    return ok?0:1;
usage:
    fputs("usage: wide-image new IMAGE SECTORS DATA_START | put IMAGE DATA_START ID EXPECTED_REV SOURCE | get IMAGE DATA_START ID DEST | stat IMAGE DATA_START ID\n",stderr);
    return 2;
}
