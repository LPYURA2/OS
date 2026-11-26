#include <styleos/stylefs_journal64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CAP (UINT64_C(1)<<34)
#define START 4096u
#define G4 (UINT64_C(1)<<32)
struct disk {
    uint8_t bytes[SFS64_JOURNAL_SECTORS][512],pending[512];
    uint64_t lba;
    unsigned writes,flushes,reads,fail;
    bool persist,dirty;
};
static bool rd(void *ctx,uint64_t lba,uint8_t out[512]){
    struct disk *d=ctx;if(lba>=SFS64_JOURNAL_SECTORS)return false;
    ++d->reads;memcpy(out,d->bytes[lba],512);return true;
}
static bool wr(void *ctx,uint64_t lba,const uint8_t in[512]){
    struct disk *d=ctx;if(lba>=SFS64_JOURNAL_SECTORS||d->dirty)return false;
    ++d->writes;d->dirty=true;d->lba=lba;memcpy(d->pending,in,512);return true;
}
static bool fl(void *ctx){
    struct disk *d=ctx;++d->flushes;
    bool fail=d->fail&&d->flushes==d->fail;
    if(d->dirty&&(!fail||d->persist))memcpy(d->bytes[d->lba],d->pending,512);
    d->dirty=false;return !fail;
}
static struct bs_service block;
static struct sfs64_journal j;
static struct disk disk;
static uint64_t gen,token;
static void attach(void){
    const struct bs_backend backend={&disk,1,CAP,true,false,rd,wr,fl};
    assert(bs_attach(&block,&backend,&gen)==BS_OK);
    assert(bs_grant_range(&block,gen,5,0,CAP,BS_READ|BS_WRITE,&token)==BS_OK);
}
static void reopen(void){
    assert(bs_detach(&block,gen)==BS_OK);
    disk.fail=0;disk.dirty=false;attach();
    memset(&j,0,sizeof j);
    assert(sfs64_journal_mount(&j,&block,5,token,CAP,START)==SJ_OK);
}
static void le32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static size_t record(uint8_t *p,uint64_t id,uint64_t first,uint64_t bytes){
    struct sfs64_file file={id,1,bytes,123};
    struct sfs64_run run={first,sfs64_sectors(bytes)};
    size_t n=sfs64_map_length(1);le32(p,(uint32_t)n);
    assert(sfs64_map_encode(p+4,n,file,CAP,START,&run,1));return n+4;
}
static void expect(uint64_t generation,const uint8_t *p,size_t n){
    assert(j.valid&&j.generation[j.active]==generation&&j.length[j.active]==n);
    if(n)assert(!memcmp(j.payload[j.active],p,n));
}
int main(void){
    attach();
    assert(sfs64_journal_mount(&j,&block,5,token,CAP,START)==SJ_SIGNATURE&&!disk.writes);
    assert(sfs64_journal_provision(&block,5,token,CAP,START)==SJ_OK);
    assert(sfs64_journal_mount(&j,&block,5,token,CAP,START)==SJ_OK);expect(1,0,0);
    unsigned old=disk.writes;
    assert(sfs64_journal_provision(&block,5,token,CAP,START)==SJ_SIGNATURE&&disk.writes==old);
    uint8_t a[168],b[168],bad[168];size_t na=record(a,1,START,G4+1);
    na+=record(a+na,2,UINT64_C(1)<<33,512);
    size_t nb=record(b,1,UINT64_C(1)<<32,G4+17);
    nb+=record(b+nb,2,UINT64_C(1)<<33,512);
    assert(na==sizeof a&&nb==sizeof b);
    memcpy(bad,a,sizeof a);record(bad+84,1,UINT64_C(1)<<33,512);
    assert(!sfs64_snapshot_valid(bad,sizeof bad,CAP,START));
    record(bad+84,2,START+1,512);
    assert(!sfs64_snapshot_valid(bad,sizeof bad,CAP,START));
    assert(sfs64_journal_publish(&j,bad,sizeof bad)==SJ_INVALID&&disk.writes==old);
    assert(sfs64_journal_publish(&j,a,na)==SJ_OK);expect(2,a,na);
    reopen();expect(2,a,na);
    uint8_t saved[SFS64_JOURNAL_SECTORS][512];memcpy(saved,disk.bytes,sizeof saved);
    for(unsigned persisted=0;persisted<2;++persisted)for(unsigned cut=1;cut<=9;++cut){
        memcpy(disk.bytes,saved,sizeof saved);reopen();expect(2,a,na);
        disk.writes=disk.flushes=0;disk.fail=cut;disk.persist=persisted!=0;
        assert(sfs64_journal_publish(&j,b,nb)==SJ_UNCERTAIN&&!j.valid);
        assert(sfs64_journal_publish(&j,b,nb)==SJ_STALE&&disk.writes==cut);
        reopen();
        if(persisted&&cut==9)expect(3,b,nb);else expect(2,a,na);
        assert(!memcmp(disk.bytes[0],saved[0],512));
        for(unsigned lba=10;lba<19;++lba)assert(!memcmp(disk.bytes[lba],saved[lba],512));
    }
    memcpy(disk.bytes,saved,sizeof saved);reopen();
    assert(sfs64_journal_publish(&j,b,nb)==SJ_OK);reopen();expect(3,b,nb);
    assert(j.generation[j.active^1u]==2);
    disk.bytes[2][7]^=1;reopen();expect(2,a,na);
    disk.bytes[11][7]^=1;
    assert(sfs64_journal_mount(&j,&block,5,token,CAP,START)==SJ_CORRUPT&&!j.valid);
    puts("STYLEFS WIDE JOURNAL PASS: two durable banks, 18 cut points, global manifest checks, preserved active bank, corrupt newest fallback, both invalid refusal");
}
