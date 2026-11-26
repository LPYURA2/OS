#include <styleos/ata_runtime_863.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {
    struct ata_runtime_863 *a;
    uint16_t identify[256],media[256],durable[256];
    unsigned calls,fail_at,command,index,commands,write_words,flushes;
    uint8_t regs[9];
    bool timeout,completion_timeout,flush_timeout;
    unsigned completion_busy,flush_busy,pauses;
    bool pause_fail,wide;
};
static bool call(struct fixture*f)
{
    assert(f->a->busy);
    return ++f->calls!=f->fail_at;
}
static bool status(void *p,uint8_t *v)
{
    struct fixture*f=p;if(!call(f))return false;
    if(f->timeout){*v=128;return true;}
    if(f->command==0x30&&f->index==256){
        if(f->completion_timeout){*v=0xd0;return true;}
        if(f->completion_busy){--f->completion_busy;*v=0xd0;return true;}
    }
    if(f->command==0xe7){
        if(f->flush_timeout){*v=0xc0;return true;}
        if(f->flush_busy){--f->flush_busy;*v=0xc0;return true;}
        memcpy(f->durable,f->media,sizeof f->media);
    }
    *v=(uint8_t)(64|((f->command==0xec||f->command==0x20||f->command==0x30)&&f->index<256?8:0));
    return true;
}
static bool reg(void *p,unsigned r,uint8_t v)
{
    struct fixture*f=p;if(!call(f))return false;
    assert((f->wide?ata857_allowed_lba28(r,v):ata857_allowed(r,v))||(r==7&&(v==0x30||v==0xe7)));
    f->regs[r]=v;
    if(r==7){f->command=v;f->index=0;f->commands++;
        if(v==0xe7)f->flushes++;
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
static bool pace(void*p)
{
    struct fixture*f=p;
    assert(f->a->busy&&(f->a->phase==ATA863_WRITE_COMPLETE||f->a->phase==ATA863_FLUSH));
    ++f->pauses;return !f->pause_fail;
}
static const struct ata_runtime_ops_863 ops={{status,reg,rd},wr,pace};
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
    assert(!ata_runtime_write_863(&a,0,in)&&a.quarantined&&f.write_words==256&&a.fault==ATA863_STATUS_TIMEOUT&&a.phase==ATA863_WRITE_COMPLETE&&a.status_polls==ATA863_COMPLETION_POLLS&&a.last_status==0xd0&&!a.pending_flush);refused++;
    unsigned stopped=f.calls;assert(!ata_runtime_flush_863(&a)&&!ata_runtime_read_863(&a,0,out)&&f.calls==stopped&&a.phase==ATA863_WRITE_COMPLETE);refused+=2;
    /* Reproduce the observed BUSY statuses for longer than the old budget.
     * Exactly one write command and one flush; no replay or fake readback. */
    a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));
    f.completion_busy=2*ATA857_POLLS;f.flush_busy=2*ATA857_POLLS;
    assert(ata_runtime_write_863(&a,20,in)&&a.pending_flush&&f.commands==2&&f.write_words==256&&!f.completion_busy);positive++;
    assert(ata_runtime_flush_863(&a)&&!a.pending_flush&&f.commands==3&&f.flushes==1&&!f.flush_busy);positive++;
    assert(ata_runtime_read_863(&a,20,out)&&f.commands==4&&!memcmp(in,out,512)&&!memcmp(f.media,f.durable,sizeof f.media));positive++;
    /* The last allowed sample succeeds; one additional BUSY sample refuses. */
    a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));
    f.completion_busy=ATA863_COMPLETION_POLLS+3;
    assert(ata_runtime_write_863(&a,20,in)&&f.commands==2&&!f.completion_busy);positive++;
    f.flush_busy=ATA863_COMPLETION_POLLS+3;
    assert(ata_runtime_flush_863(&a)&&f.commands==3&&!f.flush_busy);positive++;
    a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));
    f.completion_busy=ATA863_COMPLETION_POLLS+4;
    assert(!ata_runtime_write_863(&a,20,in)&&a.quarantined&&f.commands==2&&a.status_polls==ATA863_COMPLETION_POLLS);refused++;
    a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));
    assert(ata_runtime_write_863(&a,20,in));f.flush_timeout=true;
    assert(!ata_runtime_flush_863(&a)&&a.quarantined&&a.pending_flush&&a.phase==ATA863_FLUSH&&a.fault==ATA863_STATUS_TIMEOUT&&a.last_status==0xc0&&a.status_polls==ATA863_COMPLETION_POLLS&&f.commands==3&&f.flushes==1);refused++;
    stopped=f.calls;
    assert(!ata_runtime_flush_863(&a)&&!ata_runtime_write_863(&a,20,in)&&!ata_runtime_read_863(&a,20,out)&&stopped==f.calls);refused+=3;
    /* Pacing occurs only between BUSY completion polls. Last BUSY refuses
     * without an extra delay, and pacing failure quarantines without replay. */
    a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));
    f.completion_busy=9;f.flush_busy=11;
    assert(ata_runtime_write_863(&a,20,in)&&f.pauses==5&&f.commands==2);positive++;
    assert(ata_runtime_flush_863(&a)&&f.pauses==12&&f.commands==3);positive++;
    a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));
    f.completion_busy=5;f.pause_fail=true;
    assert(!ata_runtime_write_863(&a,20,in)&&a.quarantined&&a.fault==ATA863_PACING_IO&&
           f.pauses==1&&f.commands==2&&f.write_words==256&&!a.pending_flush);refused++;
    stopped=f.calls;assert(!ata_runtime_flush_863(&a)&&!ata_runtime_write_863(&a,20,in)&&f.calls==stopped);refused+=2;
    a=(struct ata_runtime_863){0};f=fixture(&a);assert(ata_runtime_bind_863(&a,&ops,&f));
    f.completion_timeout=true;
    assert(!ata_runtime_write_863(&a,20,in)&&f.pauses==ATA863_COMPLETION_POLLS-1&&f.commands==2);refused++;
    /* Explicit wide capacity, exact serial, high address bits, old API
     * still rejects the wide disk. These are transport model tests only. */
    const char serial[20]=ATA864_PROVISIONED_SERIAL;
    const uint32_t capacities[]={65536u,2097152u,ATA_LBA28_SECTORS};
    for(unsigned i=0;i<sizeof capacities/sizeof capacities[0];++i){
        uint32_t cap=capacities[i],lba=cap-1;
        a=(struct ata_runtime_863){0};f=fixture(&a);f.wide=true;
        f.identify[60]=(uint16_t)cap;f.identify[61]=(uint16_t)(cap>>16);
        for(unsigned j=0;j<10;++j)f.identify[10+j]=(uint16_t)(((uint16_t)(uint8_t)serial[2*j]<<8)|(uint8_t)serial[2*j+1]);
        assert(ata_runtime_bind_capacity(&a,&ops,&f,serial,cap)&&a.reader.sectors==cap);
        unsigned before=f.calls;
        memset(out,0xa5,sizeof out);
        assert(!ata_runtime_read_863(&a,cap,out)&&!ata_runtime_write_863(&a,cap,in)&&f.calls==before);
        for(unsigned j=0;j<512;++j)assert(out[j]==0xa5);
        assert(ata_runtime_write_863(&a,lba,in)&&f.regs[3]==(uint8_t)lba&&
               f.regs[4]==(uint8_t)(lba>>8)&&f.regs[5]==(uint8_t)(lba>>16)&&
               f.regs[6]==(uint8_t)(0xe0u|(lba>>24)));
        assert(ata_runtime_flush_863(&a)&&ata_runtime_read_863(&a,lba,out)&&!memcmp(in,out,512));
        assert(f.regs[3]==(uint8_t)lba&&f.regs[4]==(uint8_t)(lba>>8)&&
               f.regs[5]==(uint8_t)(lba>>16)&&f.regs[6]==(uint8_t)(0xe0u|(lba>>24)));
    }
    a=(struct ata_runtime_863){0};f=fixture(&a);f.identify[60]=0;f.identify[61]=1;
    assert(!ata_runtime_bind_863(&a,&ops,&f)&&a.quarantined&&!f.write_words);
    a=(struct ata_runtime_863){0};f=fixture(&a);
    assert(!ata_runtime_bind_capacity(&a,&ops,&f,serial,0)&&f.calls==0);
    assert(!ata_runtime_bind_capacity(&a,&ops,&f,serial,ATA_LBA28_SECTORS+1)&&f.calls==0);
    assert(!ata_runtime_bind_capacity(&a,&ops,&f,serial,65536)&&a.quarantined&&!f.write_words);
    a=(struct ata_runtime_863){0};f=fixture(&a);f.identify[60]=0;f.identify[61]=1;
    assert(!ata_runtime_bind_capacity(&a,&ops,&f,serial,65536)&&a.quarantined&&!f.write_words);
    assert(!ata857_allowed(5,1)&&!ata857_allowed(6,0xef));
    for(unsigned r=0;r<16;r++)for(unsigned v=0;v<256;v++)
        assert(ata857_allowed_lba28(r,(uint8_t)v)==(ata857_allowed(r,(uint8_t)v)||r==5||(r==6&&v>=0xe0&&v<=0xef)));
    puts("ATA CAPACITY PASS: 32MiB/1GiB/128GiB model boundaries, exact geometry/serial, old policy, full LBA28 task file, no out-of-range IO");
    printf("S863 PIO write transport: %u positive, %u refusals PASS (host model only)\n",positive,refused);
}
