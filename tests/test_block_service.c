#include <styleos/block_service.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct bs_service s;
static uint8_t in[512],out[512];
struct mock {
    uint8_t data[512];
    uint64_t last,gen,token,owner;
    unsigned reads,writes,flushes;
    bool fail_read,fail_write,fail_flush,reenter;
};
static bool rd(void *ctx,uint64_t lba,uint8_t b[512])
{
    struct mock *m=ctx;
    m->last=lba;++m->reads;
    assert(b==s.staging);
    if(m->reenter){
        assert(bs_detach(&s,m->gen)==BS_BUSY);
        assert(bs_revoke_owner(&s,m->owner)==BS_BUSY);
        assert(bs_read_sector(&s,m->owner,m->token,0,out)==BS_BUSY);
    }
    memcpy(b,m->data,m->fail_read?13:512);
    return !m->fail_read;
}
static bool wr(void *ctx,uint64_t lba,const uint8_t b[512])
{
    struct mock *m=ctx;
    m->last=lba;++m->writes;
    assert(b==s.staging);
    memcpy(m->data,b,512);
    return !m->fail_write;
}
static bool fl(void *ctx)
{
    struct mock *m=ctx;++m->flushes;return !m->fail_flush;
}
static void zeroed(void){for(unsigned i=0;i<512;++i)assert(!s.staging[i]);}
int main(void)
{
    struct mock a={0},b={0};
    const uint64_t big=UINT64_C(0x100000000)+19;
    struct bs_backend da={&a,11,UINT64_MAX,true,false,rd,wr,fl};
    struct bs_backend db={&b,22,1024,false,true,rd,wr,fl};
    uint64_t ga=0,gb=0,ta=0,tb=0,temp=99;
    assert(bs_attach(&s,&da,&ga)==BS_OK);
    assert(bs_attach(&s,&da,&temp)==BS_DENIED&&temp==99);
    assert(bs_attach(&s,&db,&gb)==BS_OK);
    db.writable=true;
    assert(bs_attach(&s,&db,&temp)==BS_INVALID&&temp==99);
    assert(bs_grant_range(&s,ga,100,UINT64_MAX-2,3,BS_READ,&temp)==BS_RANGE&&temp==99);
    assert(bs_grant_range(&s,ga,100,big,2,BS_READ|BS_WRITE,&ta)==BS_OK);
    assert(bs_grant_range(&s,ga,200,big+1,1,BS_READ,&temp)==BS_DENIED&&temp==99);
    assert(bs_grant_range(&s,gb,200,0,1024,BS_WRITE,&temp)==BS_DENIED&&temp==99);
    assert(bs_grant_range(&s,gb,200,0,1024,BS_READ,&tb)==BS_OK);
    memset(in,0x5a,512);memset(out,0xa5,512);
    assert(bs_write_sector(&s,200,ta,0,in)==BS_DENIED);
    assert(bs_write_sector(&s,100,ta,2,in)==BS_RANGE);
    assert(bs_write_sector(&s,200,tb,0,in)==BS_DENIED);
    assert(bs_read_sector(&s,100,ta,0,s.staging)==BS_INVALID);
    assert(a.writes==0&&b.writes==0&&a.reads==0&&b.reads==0);
    assert(bs_write_sector(&s,100,ta,1,in)==BS_OK&&a.last==big+1&&a.flushes==1);
    zeroed();
    a.reenter=true;a.gen=ga;a.token=ta;a.owner=100;
    assert(bs_read_sector(&s,100,ta,1,out)==BS_OK&&a.last==big+1&&!memcmp(in,out,512));
    zeroed();
    a.fail_read=true;memset(out,0xa5,512);
    assert(bs_read_sector(&s,100,ta,1,out)==BS_IO);
    for(unsigned i=0;i<512;++i)assert(out[i]==0xa5);
    zeroed();
    unsigned n=a.reads;
    assert(bs_read_sector(&s,100,ta,0,out)==BS_RECOVERY&&a.reads==n);
    assert(bs_write_sector(&s,100,ta,0,in)==BS_RECOVERY&&a.writes==1);
    assert(bs_read_sector(&s,200,tb,1023,out)==BS_OK&&b.last==1023);
    assert(bs_detach(&s,ga)==BS_OK);
    assert(bs_read_sector(&s,100,ta,0,out)==BS_STALE);
    a.fail_read=false;a.reenter=false;
    uint64_t oldgen=ga,oldtoken=ta;
    assert(bs_attach(&s,&da,&ga)==BS_OK&&ga>oldgen);
    assert(bs_grant_range(&s,oldgen,100,big,2,BS_READ,&temp)==BS_STALE);
    assert(bs_grant_range(&s,ga,100,big,2,BS_WRITE,&ta)==BS_OK&&ta>oldtoken);
    assert(bs_read_sector(&s,100,ta,0,out)==BS_DENIED);
    a.fail_flush=true;
    assert(bs_write_sector(&s,100,ta,0,in)==BS_UNCERTAIN);
    n=a.writes;
    assert(bs_write_sector(&s,100,ta,0,in)==BS_RECOVERY&&a.writes==n);
    assert(bs_revoke(&s,200,ta)==BS_DENIED);
    assert(bs_revoke_owner(&s,100)==BS_OK);
    assert(bs_write_sector(&s,100,ta,0,in)==BS_STALE);
    assert(bs_read_sector(&s,200,tb,0,out)==BS_OK);
    assert(bs_detach(&s,ga)==BS_OK);
    a.fail_flush=false;a.fail_write=true;
    assert(bs_attach(&s,&da,&ga)==BS_OK);
    assert(bs_grant_range(&s,ga,100,0,1,BS_WRITE,&ta)==BS_OK);
    n=a.flushes;
    assert(bs_write_sector(&s,100,ta,0,in)==BS_UNCERTAIN&&a.flushes==n);
    zeroed();
    assert(bs_detach(&s,ga)==BS_OK);
    assert(bs_detach(&s,gb)==BS_OK);
    assert(bs_read_sector(&s,200,tb,0,out)==BS_STALE);
    s.serial=UINT64_MAX;
    assert(bs_attach(&s,&da,&temp)==BS_FULL&&temp==99);
    puts("BLOCK SERVICE PASS: 64-bit LBA, owner/range/RO, exclusive writer, staging, reentry, stale, quarantine, peer, no retry");
    return 0;
}
