/* Production StyleFS v2: twenty-megabyte incremental cross-volume copy,
 * interruption, cancellation, conflict, no partial publication, remount.
 */
#include <styleos/stylefs.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MIB (1024u * 1024u)
enum { SECTORS = SFS_V2_MIN_SECTORS, FILE_BYTES = 20u * MIB };
struct disk { uint8_t *bytes; bool online; int32_t bad_read, bad_write; unsigned writes; };
static struct disk disks[2];
static struct sfs_volume v[2], readonly;
static struct sfs_workspace ws[2], ro_ws;
static struct sfs_io io[2];
static struct sfs_copy_job job;
static unsigned src_id;
static bool rd(void *p,uint32_t n,uint8_t out[512]) {
    struct disk *d=p;
    if(!d->online||n>=SECTORS||(int32_t)n==d->bad_read)return false;
    memcpy(out,d->bytes+(size_t)n*512,512);return true;
}
static bool wr(void *p,uint32_t n,const uint8_t in[512]) {
    struct disk *d=p;
    if(!d->online||n>=SECTORS||(int32_t)n==d->bad_write)return false;
    memcpy(d->bytes+(size_t)n*512,in,512);++d->writes;return true;
}
static bool flush(void *p){return ((struct disk*)p)->online;}
static uint8_t pattern(uint32_t o){
    uint32_t n=o^(o>>9)^(o>>16)^0x5a93u;return (uint8_t)(n*37u+(n>>5));
}
static bool produce(void *ctx,uint32_t o,uint8_t *out,size_t n){
    (void)ctx;for(size_t i=0;i<n;++i)out[i]=pattern(o+(uint32_t)i);return true;
}
static bool check(void *ctx,uint32_t o,const uint8_t *b,size_t n){
    unsigned *count=ctx;for(size_t i=0;i<n;++i)if(b[i]!=pattern(o+(uint32_t)i))return false;
    *count+=(unsigned)n;return true;
}
static void mount(unsigned i){assert(sfs_mount(&v[i],&ws[i],&io[i])==SFS_OK);}
static void absent(const char *name){assert(sfs_find(&v[1],SFS_ROOT,name)==-1);}
static void start(const char *name){
    assert(sfs_copy_job_begin(&job,&v[0],src_id,&v[1],&ws[1],SFS_ROOT,name)==SFS_OK);
    assert(job.active&&job.copied==0&&job.size==FILE_BYTES);
}
static bool step(unsigned budget){
    bool done=false;assert(sfs_copy_job_step(&job,budget,&done)==SFS_OK);return done;
}
int main(void){
    for(unsigned i=0;i<2;++i){
        disks[i].bytes=calloc(SECTORS,512);assert(disks[i].bytes);
        disks[i].online=true;disks[i].bad_read=disks[i].bad_write=-1;
        io[i]=(struct sfs_io){&disks[i],SECTORS,rd,wr,flush};
        assert(sfs_format(&v[i],&ws[i],&io[i])==SFS_OK);mount(i);
    }
    assert(sfs_create_from(&v[0],&ws[0],SFS_ROOT,"source.bin",FILE_BYTES,produce,0,&src_id)==SFS_OK);
    unsigned original_write=disks[1].writes;
    start("project-copy.bin");
    bool done=false;
    assert(sfs_copy_job_step(&job,0,&done)==SFS_STATE);
    assert(!done&&job.active&&disks[1].writes==original_write);
    for(unsigned i=0;i<128;++i){
        assert(!step(32));assert(job.copied==(i+1)*32u*512u);absent("project-copy.bin");
    }
    assert(job.copied==2*MIB);
    sfs_copy_job_cancel(&job);sfs_copy_job_cancel(&job);absent("project-copy.bin");
    mount(1);absent("project-copy.bin");
    start("project-copy.bin");
    disks[0].bad_read=(int32_t)(job.source_lba+33u);
    assert(!step(32));
    assert(sfs_copy_job_step(&job,32,&done)==SFS_IO);
    assert(!done&&!job.active&&!v[0].mounted);absent("project-copy.bin");
    disks[0].bad_read=-1;mount(0);mount(1);absent("project-copy.bin");
    start("project-copy.bin");
    disks[1].bad_write=(int32_t)(job.destination_lba+1u);
    assert(sfs_copy_job_step(&job,10,&done)==SFS_IO);
    assert(!done&&!job.active&&!v[1].mounted);
    disks[1].bad_write=-1;mount(1);absent("project-copy.bin");
    start("project-copy.bin");
    unsigned cycles=0;
    while(!done){
        done=step(64);
        assert(++cycles<=FILE_BYTES/512u+3u);
        if(!done)absent("project-copy.bin");
    }
    assert(cycles==FILE_BYTES/(64u*512u));
    assert(!job.active&&job.copied==FILE_BYTES);
    int target=sfs_find(&v[1],SFS_ROOT,"project-copy.bin");assert(target>=0);
    unsigned received=0;
    assert(sfs_read_to(&v[1],(unsigned)target,check,&received)==SFS_OK);
    assert(received==FILE_BYTES);
    mount(0);mount(1);
    assert(sfs_find(&v[1],SFS_ROOT,"project-copy.bin")==target);
    assert(sfs_find(&v[0],SFS_ROOT,"source.bin")==(int)src_id);
    assert(sfs_copy_job_begin(&job,&v[0],src_id,&v[1],&ws[1],
                              SFS_ROOT,"project-copy.bin")==SFS_EXISTS);
    assert(sfs_copy_job_begin(&job,&v[0],SFS_ROOT,&v[1],&ws[1],
                              SFS_ROOT,"folder.bin")==SFS_TYPE);
    assert(sfs_copy_job_begin(&job,&v[0],src_id,&v[0],&ws[0],
                              SFS_ROOT,"same.bin")==SFS_STATE);
    assert(sfs_copy_job_begin(&job,&v[0],src_id,&v[1],&ws[1],
                              SFS_ROOT,"too-large.bin")==SFS_FULL);
    struct sfs_io ro_io=io[1];ro_io.write=0;ro_io.flush=0;
    assert(sfs_mount(&readonly,&ro_ws,&ro_io)==SFS_OK);
    unsigned writes_before=disks[1].writes;
    assert(sfs_copy_job_begin(&job,&v[0],src_id,&readonly,&ro_ws,
                              SFS_ROOT,"denied.bin")==SFS_STATE);
    assert(disks[1].writes==writes_before);
    uint8_t small[1536];memset(small,0x5a,sizeof(small));
    unsigned small_id;
    assert(sfs_create(&v[0],&ws[0],SFS_ROOT,"small.bin",small,
                      sizeof(small),&small_id)==SFS_OK);
    assert(sfs_copy_job_begin(&job,&v[0],small_id,&v[1],&ws[1],
                              SFS_ROOT,"concurrent.bin")==SFS_OK);
    assert(!step(1));
    unsigned folder;
    assert(sfs_mkdir(&v[1],&ws[1],SFS_ROOT,"other",&folder)==SFS_OK);
    assert(sfs_copy_job_step(&job,1,&done)==SFS_STATE);
    assert(!job.active&&!done);absent("concurrent.bin");
    assert(sfs_find(&v[1],SFS_ROOT,"other")==(int)folder);
    unsigned empty;
    assert(sfs_create(&v[0],&ws[0],SFS_ROOT,"empty.txt",0,0,&empty)==SFS_OK);
    assert(sfs_copy_job_begin(&job,&v[0],empty,&v[1],&ws[1],
                              SFS_ROOT,"empty-copy.txt")==SFS_OK);
    assert(job.size==0&&step(1));
    int empty_target=sfs_find(&v[1],SFS_ROOT,"empty-copy.txt");
    assert(empty_target>=0&&sfs_entry_at(&v[1],(unsigned)empty_target)->size==0);
    mount(1);
    assert(sfs_find(&v[1],SFS_ROOT,"empty-copy.txt")==empty_target);
    for(unsigned i=0;i<2;++i)free(disks[i].bytes);
    puts("STYLEFS 20MiB STAGED COPY PASS: bounded steps, cancel, disconnect, retry, CRC,");
    puts("no partial publish, remount, readonly, conflicts, generation isolation, empty");
    return 0;
}
