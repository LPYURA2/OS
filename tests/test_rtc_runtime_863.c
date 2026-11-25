#include <styleos/rtc_runtime_863.h>
#include <assert.h>
#include <stdio.h>
struct fixture {
    struct rtc_runtime_863 *r;
    struct rtc_runtime_observation_863 o;
    unsigned samples, ports, lose_at, fail_at;
    uint8_t index;
    bool reenter;
};
static const struct rtc_time_854 sentinel={255,254,253};
static void unchanged(struct rtc_time_854 t)
{assert(t.hour==255&&t.min==254&&t.sec==253);}
static bool observe(void *ctx,struct rtc_runtime_observation_863 *out)
{
    struct fixture *f=ctx;
    ++f->samples;
    if(f->reenter){
        struct rtc_time_854 t=sentinel;
        assert(!rtc_runtime_read_863(f->r,&t));unchanged(t);
    }
    if(f->lose_at && f->samples>=f->lose_at) f->o.root=0x2000;
    *out=f->o;return true;
}
static bool write_index(void *ctx,uint8_t index)
{
    struct fixture *f=ctx;
    assert(f->r->busy&&f->r->owner.held);
    assert(!((f->index^index)&128));
    if(++f->ports==f->fail_at)return false;
    f->index=index;return true;
}
static bool read_data(void *ctx,uint8_t *out)
{
    struct fixture *f=ctx;
    assert(f->r->busy&&f->r->owner.held);
    if(++f->ports==f->fail_at)return false;
    switch(f->index&127){
    case 0:*out=45;return true;
    case 2:*out=34;return true;
    case 4:*out=12;return true;
    case 10:*out=0;return true;
    case 11:*out=6;return true;
    case 13:*out=128;return true;
    default:assert(0);return false;
    }
}
static const struct rtc_runtime_ops_863 ops={observe,write_index,read_data};
static struct fixture init(struct rtc_runtime_863 *r,uint8_t idx)
{
    return (struct fixture){.r=r,.index=idx,.reenter=true,
        .o={.root=0x1000,.epoch=7,.rflags=2,.cs=0x38,
        .q35_tcg=true,.single_cpu=true,.post_ebs=true,
        .exclusive_cmos=true,.known_shadow=true}};
}
static bool bind(struct fixture *f)
{return rtc_runtime_bind_863(f->r,&ops,f,0x1000,7,f->index);}
int main(void)
{
    unsigned positive=0,refused=0;
    for(unsigned nmi=0;nmi<=128;nmi+=128){
        struct rtc_runtime_863 r={0};struct fixture f=init(&r,(uint8_t)(nmi|0x22));
        struct rtc_time_854 t=sentinel;
        assert(!rtc_runtime_read_863(&r,&t));unchanged(t);assert(!f.ports);refused++;
        assert(bind(&f));assert(!f.ports);assert(!bind(&f));refused++;
        assert(rtc_runtime_read_863(&r,&t));
        assert(t.hour==12&&t.min==34&&t.sec==45&&f.ports==29&&f.index==(nmi|0x22));
        unsigned samples=f.samples;positive++;
        assert(rtc_runtime_read_863(&r,&t)&&f.ports==58);positive++;
        for(unsigned loss=2;loss<=samples;loss++){
            r=(struct rtc_runtime_863){0};f=init(&r,(uint8_t)(nmi|0x22));t=sentinel;
            assert(bind(&f));f.lose_at=loss;
            assert(!rtc_runtime_read_863(&r,&t));unchanged(t);
            assert(r.quarantined&&!r.busy&&!r.owner.held);
            unsigned ports=f.ports;f.lose_at=0;f.o.root=0x1000;
            assert(!rtc_runtime_read_863(&r,&t)&&!bind(&f)&&f.ports==ports);
            unchanged(t);refused+=3;
        }
        for(unsigned fail=1;fail<=29;fail++){
            r=(struct rtc_runtime_863){0};f=init(&r,(uint8_t)(nmi|0x22));t=sentinel;
            assert(bind(&f));f.fail_at=fail;
            assert(!rtc_runtime_read_863(&r,&t));unchanged(t);assert(!r.busy&&!r.owner.held);refused++;
            if(fail==29){assert(r.quarantined);unsigned p=f.ports;assert(!rtc_runtime_read_863(&r,&t)&&f.ports==p);refused++;}
            else{assert(!r.quarantined&&f.index==(nmi|0x22));f.fail_at=0;assert(rtc_runtime_read_863(&r,&t));positive++;}
        }
    }
    for(unsigned bad=0;bad<9;bad++){
        struct rtc_runtime_863 r={0};struct fixture f=init(&r,0x22);
        switch(bad){
        case 0:f.o.root=0x2000;break;case 1:f.o.epoch++;break;
        case 2:f.o.rflags|=0x200;break;case 3:f.o.cs|=3;break;
        case 4:f.o.q35_tcg=false;break;case 5:f.o.single_cpu=false;break;
        case 6:f.o.post_ebs=false;break;case 7:f.o.exclusive_cmos=false;break;
        case 8:f.o.known_shadow=false;break;
        }
        assert(!bind(&f)&&r.quarantined&&!f.ports);
        f.o=init(&r,0x22).o;assert(!bind(&f)&&!f.ports);refused+=2;
    }
    printf("S863 runtime RTC: %u positive, %u refusals PASS (host model only)\n",positive,refused);
}
