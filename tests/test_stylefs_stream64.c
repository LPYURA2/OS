#include <styleos/stylefs_stream64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CAP 8192u
#define START 32u
#define LARGE (1024u*1024u+17u)
static struct disk {uint8_t bytes[CAP][512];unsigned writes,reads,flushes;bool fail,corrupt;} disk;
static struct bs_service block;
static struct sfs64_journal journal;
static struct sfs64_stream stream,second;
static uint64_t gen,token;
static uint8_t buf[SFS64_WINDOW_BYTES],check[SFS64_WINDOW_BYTES];
static bool rd(void *ctx,uint64_t lba,uint8_t out[512]){
    struct disk *d=ctx;if(lba>=CAP)return false;++d->reads;memcpy(out,d->bytes[lba],512);return true;
}
static bool wr(void *ctx,uint64_t lba,const uint8_t in[512]){
    struct disk *d=ctx;if(lba>=CAP)return false;++d->writes;memcpy(d->bytes[lba],in,512);
    if(d->corrupt&&lba>=START)d->bytes[lba][0]^=1;
    return true;
}
static bool fl(void *ctx){struct disk *d=ctx;++d->flushes;return !d->fail;}
static void attach(void){
    const struct bs_backend backend={&disk,1,CAP,true,false,rd,wr,fl};
    assert(bs_attach(&block,&backend,&gen)==BS_OK);
    assert(bs_grant_range(&block,gen,7,0,CAP,BS_READ|BS_WRITE,&token)==BS_OK);
}
static void reopen(void){
    assert(bs_detach(&block,gen)==BS_OK);disk.fail=disk.corrupt=false;attach();
    memset(&journal,0,sizeof journal);
    assert(sfs64_journal_mount(&journal,&block,7,token,CAP,START)==SJ_OK);
}
static uint8_t pattern(uint64_t o,unsigned seed){return (uint8_t)(o^(o>>9)^(o>>17)^seed);}
static void fill(uint64_t off,size_t n,unsigned seed){for(size_t i=0;i<n;++i)buf[i]=pattern(off+i,seed);}
static uint32_t crc(uint32_t c,const uint8_t *p,size_t n){
    for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
    return c;
}
static void save(uint64_t id,uint64_t rev,uint64_t size,unsigned seed){
    assert(sfs64_stream_begin(&stream,&journal,id,rev,size)==SJ_OK);
    for(uint64_t off=0;off<size;){
        size_t n=size-off<sizeof buf?(size_t)(size-off):sizeof buf;
        fill(off,n,seed);assert(sfs64_stream_append(&stream,buf,n)==SJ_OK);off+=n;
    }
    assert(sfs64_stream_finish(&stream)==SJ_OK&&!stream.active&&!journal.writer);
}
static void verify(uint64_t id,uint64_t revision,uint64_t size,unsigned seed){
    struct sfs64_file file;
    assert(sfs64_stream_stat(&journal,id,&file)==SJ_OK&&file.revision==revision&&file.bytes==size);
    uint32_t c=UINT32_MAX;
    for(uint64_t off=0;off<size;){
        size_t n=size-off<sizeof check?(size_t)(size-off):sizeof check,done=0;
        assert(sfs64_stream_read(&journal,id,revision,off,check,n,&done)==BS_OK&&done&&done<=n);
        fill(off,done,seed);assert(!memcmp(check,buf,done));c=crc(c,check,done);off+=done;
    }
    assert(~c==file.checksum);
}
int main(void){
    attach();
    assert(sfs64_journal_provision(&block,7,token,CAP,START)==SJ_OK);
    assert(sfs64_journal_mount(&journal,&block,7,token,CAP,START)==SJ_OK);
    save(1,0,2048,1);save(2,0,2048,2);
    save(1,1,2048,3);save(3,0,2048,4);
    assert(sfs64_stream_begin(&stream,&journal,4,0,2560)==SJ_OK);
    assert(stream.count==2&&stream.runs[0].first==START&&stream.runs[0].sectors==4);
    assert(stream.runs[1].first==START+16&&stream.runs[1].sectors==1);
    unsigned writes=disk.writes;
    assert(sfs64_stream_begin(&second,&journal,5,0,512)==SJ_STALE);
    assert(sfs64_journal_publish(&journal,0,0)==SJ_STALE);
    assert(sfs64_stream_append(&stream,buf,1)==SJ_INVALID&&disk.writes==writes);
    assert(sfs64_stream_finish(&stream)==SJ_INVALID);
    fill(0,2560,5);assert(sfs64_stream_append(&stream,buf,2560)==SJ_OK);
    struct sfs64_file file;
    assert(sfs64_stream_stat(&journal,4,&file)==SJ_STALE);
    sfs64_stream_cancel(&stream);reopen();
    assert(sfs64_stream_stat(&journal,4,&file)==SJ_STALE);
    verify(1,2,2048,3);verify(2,1,2048,2);verify(3,1,2048,4);
    save(4,0,2560,5);save(5,0,LARGE,6);save(6,0,0,0);reopen();
    verify(4,1,2560,5);verify(5,1,LARGE,6);verify(6,1,0,0);
    writes=disk.writes;
    assert(sfs64_stream_begin(&stream,&journal,5,0,512)==SJ_STALE);
    assert(sfs64_stream_begin(&stream,&journal,5,2,512)==SJ_STALE);
    assert(sfs64_stream_begin(&stream,&journal,99,0,UINT64_MAX)==SJ_FULL&&disk.writes==writes);
    assert(sfs64_stream_begin(&stream,&journal,5,1,513)==SJ_OK);
    fill(0,513,7);disk.corrupt=true;
    assert(sfs64_stream_append(&stream,buf,513)==SJ_UNCERTAIN&&!journal.valid&&!stream.active);
    reopen();verify(5,1,LARGE,6);
    assert(sfs64_stream_begin(&stream,&journal,5,1,513)==SJ_OK);
    fill(0,513,8);disk.fail=true;
    assert(sfs64_stream_append(&stream,buf,513)==SJ_UNCERTAIN&&!journal.valid);
    reopen();verify(5,1,LARGE,6);
    save(5,1,513,9);reopen();verify(5,2,513,9);
    size_t done=99;
    assert(sfs64_stream_read(&journal,5,1,0,check,1,&done)==BS_STALE&&!done);
    assert(sfs64_stream_begin(&stream,&journal,7,0,512)==SJ_OK);
    assert(bs_detach(&block,gen)==BS_OK);attach();
    writes=disk.writes;
    assert(sfs64_stream_append(&stream,buf,512)==SJ_STALE&&disk.writes==writes);
    assert(sfs64_journal_mount(&journal,&block,7,token,CAP,START)==SJ_OK);
    assert(sfs64_stream_stat(&journal,7,&file)==SJ_STALE);
    verify(5,2,513,9);
    puts("STYLEFS STREAM64 PASS: fragmented transaction, 1MiB+17 actual bytes, cold reopen, CRC, replacement, cancellation, previous snapshot, failed write/readback, stale grant");
}
