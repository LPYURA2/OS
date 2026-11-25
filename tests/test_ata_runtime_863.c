#include <styleos/ata_runtime_863.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {
    struct ata_runtime_863 *a;
    uint16_t identify[256],media[256],durable[256];
    unsigned calls,fail_at,command,index,commands,write_words,flushes;
    uint8_t regs[9];
    bool timeout,completion_timeout;
};
static bool call(struct fixture*f)
{
    assert(f->a->busy);
    return ++f->calls!=f->fail_at;
}
static bool status(void *p,uint8_t *v)
{
    struct fixture*f=p;if(!call(f))return false;
    *v=(f->timeout||(f->completion_timeout&&f->command==0x30&&f->index==256))?128:(uint8_t)(64|((f->command==0xec||f->command==0x20||f->command==0x30)&&f->index<256?8:0));
    return true;
}
static bool reg(void *p,unsigned r,uint8_t v)
{
    struct fixture*f=p;if(!call(f))return false;
    assert(ata857_allowed(r,v)||(r==7&&(v==0x30||v==0xe7)));
    f->regs[r]=v;
    if(r==7){f->command=v;f->index=0;f->commands++;
        if(v==0xe7){f->flushes++;memcpy(f->durable,f->media,sizeof f->media);}
    }
    return true;
}
static bool rd(void*p,uint16_t *v)
{
    struct fixture*f=p;if(!call(f))return false;
    assert(f->index<256&&(f->command==0xec||f->command==0x20));
    *v=(f->command==0xec?f->identify:f->media)[f->index++];return true;
}
static bool wr(void*p,uint16_t v)
{
    struct fixture*f=p;if(!call(f))return false;
    assert(f->command==0x30&&f->index<256);
    f->media[f->index++]=v;f->write_words++;return true;
}
static const struct ata_runtime_ops_863 ops={{status,reg,rd},wr};
static struct fixture fixture(struct ata_runtime_863 *a)
{
    struct fixture f={.a=a};f.identify[0]=0x40;f.identify[49]=0x200;
    f.identify[60]=4096;f.identify[83]=0x5000;return f;
}
int main(void)
{
    struct ata_runtime_863 a={0};struct fixture f=fixture(&a);
    uint8_t in[512],out[512];for(unsigned i=0;i<512;i++)in[i]=(uint8_t)(i^0xa5);
    unsigned positive=0,refused=0;
    assert(!ata_runtime_write_863(&a,0,in));refused++;
    assert(ata_runtime_bind_863(&a,&ops,&f));positive++;
    unsigned base=f.calls;
    assert(!ata_runtime_bind_863(&a,&ops,&f)&&!ata_runtime_write_863(&a,4096,in)&&!ata_runtime_flush_863(&a)&&f.calls==base);refused+=3;
    assert(ata_runtime_write_863(&a,4095,in)&&a.pending_flush&&f.write_words==256);
    assert(f.regs[3]==255&&f.regs[4]==15&&!f.regs[5]);positive++;
    unsigned write_end=f.calls;
    assert(!ata_runtime_read_863(&a,4095,out)&&!ata_runtime_write_863(&a,4095,in)&&f.calls==write_end);refused+=2;
    assert(ata_runtime_flush_863(&a)&&!a.pending_flush&&f.flushes==1);positive++;
    unsigned flush_end=f.calls;
    assert(ata_runtime_read_863(&a,4095,out)&&!memcmp(in,out,512)&&!memcmp(f.media,f.durable,sizeof f.media));positive++;
    for(unsigned i=1;i<=flush_end-base;i++){
        a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));
        f.fail_at=f.calls+i;
        bool ok=ata_runtime_write_863(&a,4095,in);
        if(ok)ok=ata_runtime_flush_863(&a);
        assert(!ok&&a.quarantined&&!a.busy&&a.fault!=ATA863_NONE&&a.phase!=ATA863_IDLE);refused++;
        unsigned n=f.calls;assert(!ata_runtime_read_863(&a,0,out)&&!ata_runtime_write_863(&a,0,in)&&!ata_runtime_flush_863(&a)&&f.calls==n);refused+=3;
    }
    a=(struct ata_runtime_863){0};f=fixture(&a);f.identify[83]=0x4000;
    assert(!ata_runtime_bind_863(&a,&ops,&f)&&a.quarantined&&!f.write_words);refused++;
    a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));f.timeout=true;
    assert(!ata_runtime_write_863(&a,0,in)&&a.quarantined&&!f.write_words&&a.fault==ATA863_STATUS_TIMEOUT&&a.phase==ATA863_WRITE_SETUP&&a.status_polls==ATA857_POLLS&&a.last_status==128);refused++;
    a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));f.completion_timeout=true;
    assert(!ata_runtime_write_863(&a,0,in)&&a.quarantined&&f.write_words==256&&a.fault==ATA863_STATUS_TIMEOUT&&a.phase==ATA863_WRITE_COMPLETE&&a.status_polls==ATA857_POLLS&&a.last_status==128&&!a.pending_flush);refused++;
    unsigned stopped=f.calls;assert(!ata_runtime_flush_863(&a)&&!ata_runtime_read_863(&a,0,out)&&f.calls==stopped&&a.phase==ATA863_WRITE_COMPLETE);refused+=2;
    printf("S863 PIO write transport: %u positive, %u refusals PASS (host model only)\n",positive,refused);
}
