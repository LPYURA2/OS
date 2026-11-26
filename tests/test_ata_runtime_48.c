#include <styleos/ata_runtime_48.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {
    struct ata_runtime_48 *a;
    uint16_t identify[256],media[256],durable[256];
    unsigned calls,fail_at,index,command,commands,nregs;
    uint8_t regs[32],values[32];
    bool stuck;
};
static const char serial[20]="STYLEOS-LBA48-TEST  ";
static bool call(struct fixture *f){assert(f->a->busy);return ++f->calls!=f->fail_at;}
static bool status(void *p,uint8_t *v){
    struct fixture *f=p;if(!call(f))return false;
    if(f->stuck){*v=128;return true;}
    if(f->command==0xea)memcpy(f->durable,f->media,sizeof f->media);
    *v=(uint8_t)(64|((f->command==0xec||f->command==0x24||f->command==0x34)&&f->index<256?8:0));return true;
}
static bool reg(void *p,unsigned r,uint8_t v){
    struct fixture *f=p;if(!call(f))return false;
    assert(f->nregs<32);f->regs[f->nregs]=(uint8_t)r;f->values[f->nregs++]=v;
    if(r==7){f->command=v;f->index=0;++f->commands;}
    return true;
}
static bool rd(void *p,uint16_t *v){
    struct fixture *f=p;if(!call(f))return false;
    assert(f->index<256&&(f->command==0xec||f->command==0x24));
    *v=(f->command==0xec?f->identify:f->durable)[f->index++];return true;
}
static bool wr(void *p,uint16_t v){
    struct fixture *f=p;if(!call(f))return false;
    assert(f->index<256&&f->command==0x34);f->media[f->index++]=v;return true;
}
static const struct ata_runtime_ops_863 ops={{status,reg,rd},wr,0};
static struct fixture model(struct ata_runtime_48 *a,uint64_t cap){
    struct fixture f={.a=a};f.identify[0]=0x40;f.identify[49]=0x200;
    f.identify[83]=0x6400;
    for(unsigned i=0;i<4;++i)f.identify[100+i]=(uint16_t)(cap>>(16*i));
    for(unsigned i=0;i<10;++i)f.identify[10+i]=(uint16_t)(((uint16_t)(uint8_t)serial[2*i]<<8)|(uint8_t)serial[2*i+1]);
    return f;
}
static void trace(const struct fixture *f,uint64_t lba,uint8_t command){
    const uint8_t regs[]={8,6,1,2,3,4,5,1,2,3,4,5,7};
    const uint8_t values[]={2,0xe0,0,0,(uint8_t)(lba>>24),(uint8_t)(lba>>32),(uint8_t)(lba>>40),
        0,1,(uint8_t)lba,(uint8_t)(lba>>8),(uint8_t)(lba>>16),command};
    assert(f->nregs==sizeof regs&&!memcmp(f->regs,regs,sizeof regs)&&!memcmp(f->values,values,sizeof values));
}
int main(void){
    struct ata_runtime_48 a={0};struct fixture f;
    uint8_t in[512],out[512];
    for(unsigned i=0;i<512;++i)in[i]=(uint8_t)(i^(i>>4));
    const uint64_t caps[]={UINT64_C(256)*1024*1024*1024/512,
        UINT64_C(512)*1024*1024*1024/512,UINT64_C(1)<<31,UINT64_C(1)<<33,ATA48_SECTORS};
    for(unsigned i=0;i<sizeof caps/sizeof caps[0];++i){
        uint64_t cap=caps[i],lba=cap-1;
        a=(struct ata_runtime_48){0};f=model(&a,cap);
        assert(ata48_bind(&a,&ops,&f,serial,cap)&&a.sectors==cap);
        unsigned before=f.calls;f.nregs=0;memset(out,0xa5,512);
        assert(!ata48_write(&a,cap,in)&&!ata48_read(&a,cap,out)&&!ata48_flush(&a)&&f.calls==before);
        for(unsigned j=0;j<512;++j)assert(out[j]==0xa5);
        assert(ata48_write(&a,lba,in)&&a.pending_flush);trace(&f,lba,0x34);
        before=f.calls;assert(!ata48_read(&a,lba,out)&&!ata48_write(&a,lba,in)&&f.calls==before);
        f.nregs=0;assert(ata48_flush(&a)&&!a.pending_flush);
        assert(f.nregs==3&&f.regs[2]==7&&f.values[2]==0xea);
        f.nregs=0;assert(ata48_read(&a,lba,out)&&!memcmp(in,out,512));trace(&f,lba,0x24);
    }
    const uint64_t cap=UINT64_C(1)<<33,lba=UINT64_C(0x123456789);
    a=(struct ata_runtime_48){0};f=model(&a,cap);
    assert(ata48_bind(&a,&ops,&f,serial,cap));f.nregs=0;
    unsigned begin=f.calls;assert(ata48_write(&a,lba,in));trace(&f,lba,0x34);
    f.nregs=0;assert(ata48_flush(&a));unsigned count=f.calls-begin;
    for(unsigned i=1;i<=count;++i){
        a=(struct ata_runtime_48){0};f=model(&a,cap);
        assert(ata48_bind(&a,&ops,&f,serial,cap));f.nregs=0;f.fail_at=f.calls+i;
        bool ok=ata48_write(&a,lba,in);
        if(ok){f.nregs=0;ok=ata48_flush(&a);}
        assert(!ok&&a.quarantined&&!a.busy);
        unsigned before=f.calls;assert(!ata48_write(&a,lba,in)&&!ata48_read(&a,lba,out)&&!ata48_flush(&a)&&f.calls==before);
    }
    a=(struct ata_runtime_48){0};f=model(&a,cap);f.identify[83]&=(uint16_t)~0x0400u;
    assert(!ata48_bind(&a,&ops,&f,serial,cap)&&a.quarantined);
    a=(struct ata_runtime_48){0};f=model(&a,cap);f.identify[83]&=(uint16_t)~0x2000u;
    assert(!ata48_bind(&a,&ops,&f,serial,cap)&&a.quarantined);
    a=(struct ata_runtime_48){0};f=model(&a,cap);f.identify[106]=0x5000;
    assert(!ata48_bind(&a,&ops,&f,serial,cap)&&a.quarantined);
    a=(struct ata_runtime_48){0};f=model(&a,cap);f.identify[10]^=1;
    assert(!ata48_bind(&a,&ops,&f,serial,cap)&&a.quarantined);
    a=(struct ata_runtime_48){0};f=model(&a,cap);
    assert(!ata48_bind(&a,&ops,&f,serial,cap+1)&&a.quarantined);
    a=(struct ata_runtime_48){0};f=model(&a,cap);assert(!ata48_bind(&a,&ops,&f,serial,ATA48_SECTORS+1)&&!f.calls);
    assert(ata48_bind(&a,&ops,&f,serial,cap));f.nregs=0;f.stuck=true;
    memset(out,0xa5,512);assert(!ata48_read(&a,lba,out)&&a.quarantined);
    for(unsigned i=0;i<512;++i)assert(out[i]==0xa5);
    assert(!ata857_allowed(7,0x24)&&!ata857_allowed_lba28(7,0x34)&&!ata857_allowed_lba28(7,0xea));
    printf("ATA48 PASS: 256GiB/512GiB/1TiB/4TiB/max geometry, high-first taskfile, durable readback, %u callback failures, old allowlist unchanged (model only)\n",count);
}
