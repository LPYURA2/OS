/* Explicit host-only T-P1.8B fixture writer. The disk format is implemented by
 * the UNCHANGED 1.8A storage.c, never duplicated here. Only a new exclusive
 * regular file is created; existing disks and host devices cannot be opened.
 * Usage: pack_disk_app APP_FILE NEW_DISK (parent directory must already exist).
 * Deliberately accepts malformed executable bytes for loader-refusal fixtures. */
#define _POSIX_C_SOURCE 200809L
#include "styleos/storage.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct fixture { int fd; unsigned reads,writes,flushes; };
static _Alignas(4096) union st_workspace workspace;
static struct st_store store;
static struct st_record record;

static int transfer(int fd,void *buffer,size_t bytes,off_t offset,int writing)
{
    unsigned char *p=buffer;size_t done=0;
    while(done<bytes){
        ssize_t n=writing?pwrite(fd,p+done,bytes-done,offset+(off_t)done):
                          pread(fd,p+done,bytes-done,offset+(off_t)done);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return -1;
        done+=(size_t)n;
    }
    return 0;
}
static int read_sector(void *opaque,uint64_t lba,void *data)
{
    struct fixture *f=opaque;if(lba>=ST_DISK_SECTORS)return -1;
    ++f->reads;return transfer(f->fd,data,ST_SECTOR_BYTES,(off_t)(lba*ST_SECTOR_BYTES),0);
}
static int write_sector(void *opaque,uint64_t lba,const void *data)
{
    struct fixture *f=opaque;if(lba>=ST_DISK_SECTORS)return -1;
    ++f->writes;
    return transfer(f->fd,(void *)data,ST_SECTOR_BYTES,(off_t)(lba*ST_SECTOR_BYTES),1);
}
static int flush(void *opaque)
{ struct fixture *f=opaque;++f->flushes;return fsync(f->fd); }

/* Walk all parent components with descriptor-relative O_NOFOLLOW opens.
 * O_EXCL protects the leaf as well: symlink parents cannot redirect creation.
 * The fixture runner supplies its fresh run/disks directory as this parent. */
static int parent_directory(const char *path,char *leaf,size_t capacity)
{
    char copy[4096];size_t n=strlen(path);
    if(!n || n>=sizeof copy){errno=EINVAL;return -1;}
    memcpy(copy,path,n+1);char *last=strrchr(copy,'/'),*name=last?last+1:copy;
    if(!*name || !strcmp(name,".") || !strcmp(name,"..") || strlen(name)>=capacity){
        errno=EINVAL;return -1;
    }
    memcpy(leaf,name,strlen(name)+1);
    int current=open(path[0]=='/'?"/":".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
    if(current<0)return -1;
    if(!last)return current;
    *last=0;char *save=NULL;
    for(char *part=strtok_r(copy,"/",&save);part;part=strtok_r(NULL,"/",&save)){
        if(!strcmp(part,"."))continue;
        if(!strcmp(part,"..")){close(current);errno=EINVAL;return -1;}
        int next=openat(current,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
        if(next<0){int error=errno;close(current);errno=error;return -1;}
        close(current);current=next;
    }
    return current;
}
int main(int argc,char **argv)
{
    if(argc!=3){fprintf(stderr,"usage: pack_disk_app APP_FILE NEW_DISK\n");return 2;}
    int app=open(argv[1],O_RDONLY|O_NOFOLLOW);struct stat status;
    if(app<0){perror("fixture application open");return 1;}
    if(fstat(app,&status) || !S_ISREG(status.st_mode) || status.st_size<1 ||
       status.st_size>ST_DATA_MAX){
        fprintf(stderr,"fixture application must be a regular file of 1..1024 bytes\n");
        close(app);return 1;
    }
    size_t length=(size_t)status.st_size;unsigned char bytes[ST_DATA_MAX];
    if(transfer(app,bytes,length,0,0)){perror("fixture application read");close(app);return 1;}
    if(close(app)){perror("fixture application close");return 1;}
    char leaf[4096];int parent=parent_directory(argv[2],leaf,sizeof leaf);
    if(parent<0){perror("fixture new disk parent");return 1;}
    struct fixture f={openat(parent,leaf,O_CREAT|O_EXCL|O_NOFOLLOW|O_RDWR,0600),0,0,0};
    if(f.fd<0){perror("fixture exclusive new disk");close(parent);return 1;}
    int result=1;
    if(fstat(f.fd,&status) || !S_ISREG(status.st_mode) || status.st_nlink!=1 ||
       ftruncate(f.fd,(off_t)(ST_DISK_SECTORS*ST_SECTOR_BYTES))){
        perror("fixture regular disk size");goto done;
    }
    const struct st_io io={&f,ST_DISK_SECTORS,read_sector,write_sector,flush};
    struct st_pending token;enum st_error error=st_init(&io,&workspace,&store);
    if(error==ST_OK)error=st_prepare(&store,&workspace,(const unsigned char *)"app.bin",7,
                                   bytes,length,&token);
    if(error==ST_OK)error=st_commit(&store,&workspace,&token);
    /* Read-only round trip proves the fixture is a valid 1.8A store, including
     * when its contained executable is intentionally rejected by the loader. */
    if(error==ST_OK)error=st_mount(&io,&workspace,&store);
    if(error==ST_OK)error=st_read(&store,&workspace,&record);
    if(error!=ST_OK){fprintf(stderr,"fixture storage: %s\n",st_error_name(error));goto done;}
    if(record.generation!=1 || record.name_length!=7 || record.length!=length ||
       memcmp(record.name,"app.bin",8) || memcmp(record.data,bytes,length) ||
       f.writes!=6 || f.flushes!=4){fprintf(stderr,"fixture round trip mismatch\n");goto done;}
    printf("{\"file\":\"app.bin\",\"file_length\":%zu,\"generation\":1,"
           "\"disk_bytes\":1048576,\"reads\":%u,\"writes\":%u,\"flushes\":%u}\n",
           length,f.reads,f.writes,f.flushes);
    result=0;
done:
    if(close(f.fd)){perror("fixture disk close");result=1;}
    if(result && unlinkat(parent,leaf,0))perror("fixture failed disk cleanup");
    close(parent);return result;
}
