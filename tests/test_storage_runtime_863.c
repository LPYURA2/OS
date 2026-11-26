#include <styleos/storage_runtime_863.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {
    struct storage_runtime_863 *s;
    struct storage_obs_863 o;
    uint8_t media[512],durable[512];
    unsigned reads,writes,flushes,samples,loss,fail;
    uint32_t last;
    bool corrupt;
};
static bool observe(void *p,struct storage_obs_863 *o)
{
    struct fixture *f=p;uint8_t out[512]={0};
    assert(!storage_read_863(f->s,0,out)&&!storage_commit_863(f->s,0,out));
    if(++f->samples==f->loss)f->o.epoch++;
    *o=f->o;return true;
}
static bool read_sector(void *p,uint32_t lba,uint8_t *out)
{
    struct fixture*f=p;assert(f->s->busy);f->reads++;f->last=lba;
    memset(out,0xab,512);if(f->fail==1)return false;
    memcpy(out,f->media,512);if(f->corrupt)out[511]^=1;return true;
}
static bool write_sector(void *p,uint32_t lba,const uint8_t *in)
{
    struct fixture*f=p;assert(f->s->busy);f->writes++;f->last=lba;
    memcpy(f->media,in,512);return f->fail!=2;
}
static bool flush(void*p)
{
    struct fixture*f=p;assert(f->s->busy);f->flushes++;
    if(f->fail==3)return false;
    memcpy(f->durable,f->media,512);return true;
}
static const struct storage_ops_863 ops={observe,read_sector,write_sector,flush};
static struct fixture fixture(struct storage_runtime_863*s)
{
    return (struct fixture){.s=s,.o={.root=0x1000,.epoch=1,.device=42,
        .sectors=4096,.post_ebs=true,.single_cpu=true,.kernel_context=true,
        .exclusive_pio=true,.dma_denied=true,.disposable_data=true,.flush_supported=true}};
}
static bool bind(struct fixture*f,bool w)
{return storage_bind_863(f->s,&ops,f,0x1000,1,42,8,16,w);}
static unsigned calls(struct fixture*f){return f->reads+f->writes+f->flushes;}
int main(void)
{
    uint8_t src[512],out[512],zero[512]={0};for(unsigned i=0;i<512;i++)src[i]=(uint8_t)(i^0x73);
    struct storage_runtime_863 s={0};struct fixture f=fixture(&s);
    unsigned good=0,bad=0;
    assert(!storage_read_863(&s,0,out)&&!storage_commit_863(&s,0,src));bad+=2;
    assert(bind(&f,true));assert(!bind(&f,true)&&!calls(&f));bad++;
    assert(!storage_read_863(&s,16,out)&&!storage_commit_863(&s,UINT32_MAX,src)&&!calls(&f));bad+=2;
    assert(storage_commit_863(&s,15,src)&&f.last==23&&f.reads==1&&f.writes==1&&f.flushes==1);
    assert(!memcmp(f.durable,src,512));unsigned samples=f.samples;good++;
    assert(storage_read_863(&s,15,out)&&!memcmp(src,out,512));good++;
    for(unsigned at=2;at<=samples;at++){
        s=(struct storage_runtime_863){0};f=fixture(&s);assert(bind(&f,true));f.loss=at;
        assert(!storage_commit_863(&s,0,src)&&s.quarantined&&!s.busy);bad++;
        unsigned n=calls(&f);assert(!storage_commit_863(&s,0,src)&&!storage_read_863(&s,0,out)&&calls(&f)==n);bad+=2;
    }
    for(unsigned fail=1;fail<=4;fail++){
        s=(struct storage_runtime_863){0};f=fixture(&s);assert(bind(&f,true));f.fail=fail;f.corrupt=fail==4;
        assert(!storage_commit_863(&s,0,src)&&s.quarantined&&!s.busy);bad++;
        if(fail==2)assert(!f.flushes&&!f.reads);
        if(fail==3)assert(!f.reads);
        unsigned n=calls(&f);assert(!storage_commit_863(&s,0,src)&&calls(&f)==n);bad++;
    }
    s=(struct storage_runtime_863){0};f=fixture(&s);assert(bind(&f,false));
    assert(!storage_commit_863(&s,0,src)&&!calls(&f));bad++;
    assert(storage_read_863(&s,0,out)&&!memcmp(out,zero,512));good++;
    f.fail=1;memcpy(out,src,512);assert(!storage_read_863(&s,0,out)&&!memcmp(out,src,512)&&s.quarantined);bad++;
    for(unsigned i=0;i<11;i++){
        s=(struct storage_runtime_863){0};f=fixture(&s);
        switch(i){
        case 0:f.o.root++;break;case 1:f.o.epoch++;break;case 2:f.o.device++;break;
        case 3:f.o.sectors++;break;case 4:f.o.post_ebs=false;break;
        case 5:f.o.single_cpu=false;break;case 6:f.o.kernel_context=false;break;
        case 7:f.o.exclusive_pio=false;break;case 8:f.o.dma_denied=false;break;
        case 9:f.o.disposable_data=false;break;case 10:f.o.flush_supported=false;break;
        }
        assert(!bind(&f,true)&&s.quarantined&&!calls(&f));bad++;
        f.o=fixture(&s).o;assert(!bind(&f,true)&&!calls(&f));bad++;
    }
    s=(struct storage_runtime_863){0};f=fixture(&s);
    assert(!storage_bind_863(&s,&ops,&f,0x1000,1,42,4095,2,true));
    assert(!storage_bind_863(&s,&ops,&f,0x1000,1,42,0,0,true));bad+=2;
    s=(struct storage_runtime_863){0};f=fixture(&s);f.o.sectors=65537;
    assert(storage_bind_capacity_863(&s,&ops,&f,0x1000,1,42,65537,1,65535,true));
    assert(storage_commit_863(&s,65534,src)&&f.last==65535);
    unsigned oldcalls=calls(&f);
    assert(!storage_commit_863(&s,65535,src)&&calls(&f)==oldcalls);
    f.o.sectors=65536;
    assert(!storage_read_863(&s,0,out)&&s.quarantined&&calls(&f)==oldcalls);
    /* The finite callback budget is renewable only between operations.
     * Simulate many completed requests and guard against reentrant reset. */
    s=(struct storage_runtime_863){0};f=fixture(&s);assert(bind(&f,true));
    unsigned used=200000;
    assert(storage_budget_begin_864(&s,true,&used)&&used==0);good++;
    for(unsigned i=0;i<1024;i++){
        used=200000;assert(storage_budget_begin_864(&s,true,&used)&&!used);
        assert(storage_read_863(&s,0,out));good++;
    }
    used=123;assert(!storage_budget_begin_864(&s,false,&used)&&used==123);bad++;
    s.busy=true;assert(!storage_budget_begin_864(&s,true,&used)&&used==123);s.busy=false;bad++;
    s.quarantined=true;assert(!storage_budget_begin_864(&s,true,&used)&&used==123);s.quarantined=false;bad++;
    s.fault=STORAGE863_GUARD;assert(!storage_budget_begin_864(&s,true,&used)&&used==123);s.fault=STORAGE863_NONE;bad++;
    s.bound=false;assert(!storage_budget_begin_864(&s,true,&used)&&used==123);bad++;
    assert(!storage_budget_begin_864(0,true,&used)&&!storage_budget_begin_864(&s,true,0));bad+=2;
    printf("S863 storage runtime: %u positive, %u refusals PASS (host model only)\n",good,bad);
}
