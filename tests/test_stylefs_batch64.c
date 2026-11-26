#include <styleos/stylefs_stream64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { CAP=256, START=32 };
static struct disk {
    uint8_t durable[CAP][512],pending[CAP][512];
    bool dirty[CAP],fail_flush;
    unsigned writes,flushes,reads,fail_write;
} d;
static struct bs_service b;
static struct sfs64_journal j;
static struct sfs64_stream t;
static uint64_t gen,token;
static uint8_t input[32768],out[32768];
static bool rd(void *p,uint64_t lba,uint8_t s[512]){
    struct disk *x=p;if(lba>=CAP)return false;++x->reads;memcpy(s,x->durable[lba],512);return true;
}
static bool wr(void *p,uint64_t lba,const uint8_t s[512]){
    struct disk *x=p;if(lba>=CAP)return false;
    if(++x->writes==x->fail_write)return false;
    x->dirty[lba]=true;memcpy(x->pending[lba],s,512);return true;
}
static bool fl(void *p){
    struct disk *x=p;++x->flushes;if(x->fail_flush)return false;
    for(unsigned i=0;i<CAP;++i)if(x->dirty[i]){memcpy(x->durable[i],x->pending[i],512);x->dirty[i]=false;}
    return true;
}
static void attach(void){
    struct bs_backend backend={&d,1,CAP,true,false,rd,wr,fl};
    assert(bs_attach(&b,&backend,&gen)==BS_OK);
    assert(bs_grant_range(&b,gen,1,0,CAP,BS_READ|BS_WRITE,&token)==BS_OK);
}
static void reopen(void){
    assert(bs_detach(&b,gen)==BS_OK);
    memset(d.dirty,0,sizeof d.dirty);d.fail_write=0;d.fail_flush=false;
    attach();assert(sfs64_journal_mount(&j,&b,1,token,CAP,START)==SJ_OK);
}
int main(void){
    attach();assert(sfs64_journal_provision(&b,1,token,CAP,START)==SJ_OK);
    assert(sfs64_journal_mount(&j,&b,1,token,CAP,START)==SJ_OK);
    for(unsigned i=0;i<sizeof input;++i)input[i]=(uint8_t)(i^(i>>9));
    unsigned writes=d.writes,flushes=d.flushes;
    assert(bs_write_staged_batch(&b,2,token,START,input,512)==BS_DENIED);
    assert(bs_write_staged_batch(&b,1,token,CAP-1,input,513)==BS_RANGE);
    assert(bs_write_staged_batch(&b,1,token,UINT64_MAX,input,513)==BS_RANGE);
    assert(bs_write_staged_batch(&b,1,token,START,input,sizeof input+1)==BS_INVALID);
    assert(d.writes==writes&&d.flushes==flushes);
    assert(sfs64_stream_begin(&t,&j,1,0,sizeof input)==SJ_OK);
    assert(sfs64_stream_append_batched(&t,input,sizeof input)==SJ_OK);
    assert(d.writes==writes+64&&d.flushes==flushes+1);
    assert(sfs64_stream_finish(&t)==SJ_OK);reopen();
    size_t done=0;
    assert(sfs64_stream_read(&j,1,1,0,out,sizeof out,&done)==BS_OK&&done==sizeof out&&!memcmp(input,out,done));
    for(unsigned failure=1;failure<=3;++failure){
        assert(sfs64_stream_begin(&t,&j,1,1,1025)==SJ_OK);
        writes=d.writes;flushes=d.flushes;
        d.fail_write=writes+failure;
        assert(sfs64_stream_append_batched(&t,input,1025)==SJ_UNCERTAIN&&!j.valid&&!t.active);
        assert(d.writes==writes+failure&&d.flushes==flushes);
        assert(bs_write_staged_batch(&b,1,token,START,input,512)==BS_RECOVERY);
        reopen();assert(sfs64_stream_read(&j,1,1,0,out,sizeof out,&done)==BS_OK&&!memcmp(input,out,sizeof out));
    }
    assert(sfs64_stream_begin(&t,&j,1,1,1025)==SJ_OK);
    writes=d.writes;flushes=d.flushes;d.fail_flush=true;
    assert(sfs64_stream_append_batched(&t,input,1025)==SJ_UNCERTAIN&&!j.valid);
    assert(d.writes==writes+3&&d.flushes==flushes+1);
    reopen();assert(sfs64_stream_read(&j,1,1,0,out,sizeof out,&done)==BS_OK&&!memcmp(input,out,sizeof out));
    assert(sfs64_stream_begin(&t,&j,1,1,1025)==SJ_OK);
    uint64_t first=t.runs[0].first;
    assert(sfs64_stream_append_batched(&t,input,1025)==SJ_OK);
    assert(d.durable[first+2][0]==input[1024]);
    for(unsigned i=1;i<512;++i)assert(!d.durable[first+2][i]);
    assert(sfs64_stream_finish(&t)==SJ_OK);reopen();
    assert(sfs64_stream_read(&j,1,2,0,out,1025,&done)==BS_OK&&done==1025&&!memcmp(input,out,done));
    puts("STYLEFS BATCH PASS: 64 writes/one flush, entire range check, every failed write, failed flush, old revision, zero tail, cold reopen");
}
