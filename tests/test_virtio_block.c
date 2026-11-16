/* Host transport model, not DMA/hardware evidence. Runs the same negotiation,
 * split-queue, sector checks and completion engine as the ARM guest. */
#include "styleos/virtio_block.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct descriptor { uint64_t address; uint32_t length; uint16_t flags,next; };
struct header { uint32_t type,reserved; uint64_t sector; };
struct used { uint32_t id,length; };
struct model {
    uint32_t regs[128],feature_select,driver_select,offered[2],selected[2];
    uint32_t generation,version,id,magic;
    uint64_t capacity;
    unsigned reset_stuck,reject_features,queue_refused,never_complete;
    unsigned bad_id,bad_length,bad_status,needs_reset,config_changes,extra_used;
    unsigned notified,pending,notifications,read_ops,write_ops,flush_ops;
    uint16_t seen;
    uint64_t desc,avail,used;
};
static unsigned char disk[2048][512];
static struct model m;

static uint32_t read_reg(void *ctx,uint32_t offset)
{
    struct model *s=ctx;
    switch(offset){
    case 0:return s->magic;
    case 4:return s->version;
    case 8:return s->id;
    case 0x10:return s->offered[s->feature_select];
    case 0x34:return s->regs[0x34/4];
    case 0xfc:return s->generation;
    case 0x100:return (uint32_t)s->capacity;
    case 0x104:return (uint32_t)(s->capacity>>32);
    default:assert(offset<512 && !(offset&3));return s->regs[offset/4];
    }
}
static void write_reg(void *ctx,uint32_t offset,uint32_t value)
{
    struct model *s=ctx;assert(offset<512 && !(offset&3));
    if(offset==0x14){assert(value<=1);s->feature_select=value;}
    else if(offset==0x24){assert(value<=1);s->driver_select=value;}
    else if(offset==0x20)s->selected[s->driver_select]=value;
    else if(offset==0x70){
        if(!value){
            s->regs[offset/4]=s->reset_stuck?1:0;
            s->regs[0x44/4]=0;s->seen=0;s->pending=0;
        }else s->regs[offset/4]=s->reject_features?(value&~8u):value;
        return;
    }else if(offset==0x44 && s->queue_refused)value=0;
    else if(offset==0x80)s->desc=(s->desc&UINT64_C(0xffffffff00000000))|value;
    else if(offset==0x84)s->desc=(s->desc&UINT64_C(0xffffffff))|((uint64_t)value<<32);
    else if(offset==0x90)s->avail=(s->avail&UINT64_C(0xffffffff00000000))|value;
    else if(offset==0x94)s->avail=(s->avail&UINT64_C(0xffffffff))|((uint64_t)value<<32);
    else if(offset==0xa0)s->used=(s->used&UINT64_C(0xffffffff00000000))|value;
    else if(offset==0xa4)s->used=(s->used&UINT64_C(0xffffffff))|((uint64_t)value<<32);
    else if(offset==0x50){assert(value==0);s->pending=1;++s->notifications;}
    else if(offset==0x64){s->regs[0x60/4]&=~value;return;}
    s->regs[offset/4]=value;
}
static void poll_device(void *ctx)
{
    struct model *s=ctx;
    if(!s->pending)return;
    if(s->needs_reset){s->regs[0x70/4]|=64;return;}
    if(s->never_complete)return;
    assert(s->desc==(uintptr_t)styleos_vblk_queue);
    assert(s->avail==(uintptr_t)styleos_vblk_queue+VB_AVAIL_OFFSET);
    assert(s->used==(uintptr_t)styleos_vblk_queue+VB_USED_OFFSET);
    assert(s->regs[0x38/4]==8 && s->regs[0x44/4]==1);
    volatile uint16_t *avail=(volatile uint16_t *)(uintptr_t)s->avail;
    volatile uint16_t *used=(volatile uint16_t *)(uintptr_t)s->used;
    assert(avail[0]==1 && avail[1]==(uint16_t)(s->seen+1));
    assert(avail[2+s->seen%8]==0);
    struct descriptor *desc=(struct descriptor *)(uintptr_t)s->desc;
    struct header *h=(struct header *)(uintptr_t)desc[0].address;
    assert(desc[0].address==(uintptr_t)styleos_vblk_request+VB_HEADER_OFFSET);
    assert(desc[0].length==16 && desc[0].flags==1 && h->reserved==0);
    assert(desc[2].address==(uintptr_t)styleos_vblk_request+VB_STATUS_OFFSET);
    assert(desc[2].length==1 && desc[2].flags==2 && desc[2].next==0);
    unsigned char *status=(unsigned char *)(uintptr_t)desc[2].address;
    assert(*status==255); /* Driver must reset status before every submission. */
    uint32_t length=1;
    if(h->type==4){assert(desc[0].next==2 && h->sector==0);++s->flush_ops;}
    else{
        assert(desc[0].next==1 && desc[1].next==2 && desc[1].length==512);
        assert(desc[1].address==(uintptr_t)styleos_vblk_request+VB_DATA_OFFSET);
        assert(h->sector<2048 && h->sector<s->capacity);
        void *data=(void *)(uintptr_t)desc[1].address;
        if(h->type==0){assert(desc[1].flags==3);memcpy(data,disk[h->sector],512);length=513;++s->read_ops;}
        else {assert(h->type==1 && desc[1].flags==1);memcpy(disk[h->sector],data,512);++s->write_ops;}
    }
    *status=s->bad_status?1:0;
    volatile struct used *u=(volatile struct used *)((uintptr_t)s->used+4+8u*(s->seen%8));
    u->id=s->bad_id?1:0;u->length=s->bad_length?length+1:length;
    ++s->seen;used[1]=(uint16_t)(s->seen+(s->extra_used?1:0));
    s->pending=0;
    if(s->config_changes)++s->generation;
}
static enum vb_error open_model(void)
{
    const struct vb_model_bus bus={&m,read_reg,write_reg,poll_device};
    return vb_model_open(&bus);
}
static void defaults(void)
{
    memset(&m,0,sizeof m);m.magic=0x74726976;m.version=2;m.id=2;
    m.offered[0]=1u<<9;m.offered[1]=1;m.capacity=2048;m.regs[0x34/4]=128;
}
static void negotiated(void)
{
    defaults();assert(open_model()==VB_OK);
    assert(m.feature_select==1 && m.driver_select==1);
    assert(m.selected[0]==(1u<<9) && m.selected[1]==1);
    assert(vb_features()==VB_REQUIRED_FEATURES && vb_offered_features()==VB_REQUIRED_FEATURES);
    assert(vb_capacity()==2048 && vb_queue_size()==8 && vb_mmio_base()==0x0a000000);
    assert(m.regs[0x70/4]==15);
}
static void negotiation_refusals(void)
{
    defaults();m.magic=0;assert(open_model()==VB_DEVICE);
    defaults();m.id=1;assert(open_model()==VB_DEVICE);
    defaults();m.version=1;assert(open_model()==VB_VERSION);
    defaults();m.offered[1]=0;assert(open_model()==VB_FEATURES);
    defaults();m.offered[0]=0;assert(open_model()==VB_FEATURES);
    defaults();m.offered[0]|=1u<<5;assert(open_model()==VB_READ_ONLY);
    defaults();m.reject_features=1;assert(open_model()==VB_FEATURES);
    defaults();m.regs[0x34/4]=4;assert(open_model()==VB_QUEUE);
    defaults();m.queue_refused=1;assert(open_model()==VB_QUEUE);
    defaults();m.reset_stuck=1;assert(open_model()==VB_RESET);
    defaults();m.capacity=0;assert(open_model()==VB_CAPACITY);
    defaults();m.capacity=UINT64_MAX;assert(open_model()==VB_CAPACITY);
    assert(vb_model_open(0)==VB_ARGUMENT);
    struct vb_model_bus bus={0};assert(vb_model_open(&bus)==VB_ARGUMENT);
}
static void sector_roundtrip_and_wrap(void)
{
    negotiated();unsigned char in[512],out[512];
    for(unsigned i=0;i<512;++i)in[i]=(unsigned char)(i^0x5a);
    for(unsigned i=0;i<40;++i){
        assert(vb_write(i,in)==VB_OK);assert(vb_read(i,out)==VB_OK);
        assert(memcmp(in,out,512)==0);
    }
    assert(vb_write(2047,in)==VB_OK && vb_flush()==VB_OK && vb_read(2047,out)==VB_OK);
    assert(memcmp(in,out,512)==0);
    assert(vb_reads()==41 && vb_writes()==41 && vb_flushes()==1 && vb_requests()==83);
    assert(m.read_ops==41 && m.write_ops==41 && m.flush_ops==1);
}
static void refusals_do_not_submit_or_modify_output(void)
{
    negotiated();unsigned char out[512];memset(out,0xc3,512);
    unsigned char q[4096],req[4096],ctl[4096];
    memcpy(q,styleos_vblk_queue,4096);memcpy(req,styleos_vblk_request,4096);
    memcpy(ctl,styleos_vblk_control.bytes,4096);
    assert(vb_read(2048,out)==VB_RANGE && vb_write(UINT64_MAX,out)==VB_RANGE);
    assert(vb_read(0,0)==VB_ARGUMENT && vb_write(0,0)==VB_ARGUMENT);
    assert(vb_read(0,styleos_vblk_queue)==VB_ARGUMENT);
    assert(vb_write(0,styleos_vblk_request)==VB_ARGUMENT);
    assert(vb_read(0,&styleos_vblk_control)==VB_ARGUMENT);
    assert(vb_read(0,(void *)(uintptr_t)(UINTPTR_MAX-8))==VB_ARGUMENT);
    assert(m.notifications==0 && vb_requests()==0);
    assert(memcmp(q,styleos_vblk_queue,4096)==0 && memcmp(req,styleos_vblk_request,4096)==0);
    assert(memcmp(ctl,styleos_vblk_control.bytes,4096)==0);
    for(unsigned i=0;i<512;++i)assert(out[i]==0xc3);
    vb_model_request_count(VB_MAX_REQUESTS);
    assert(vb_flush()==VB_LIMIT && vb_requests()==VB_MAX_REQUESTS && m.notifications==0);
}
static void completion_errors_are_terminal(void)
{
    unsigned char out[512];
    for(unsigned which=0;which<7;++which){
        negotiated();memset(out,0x7e,sizeof out);
        enum vb_error expected;
        switch(which){
        case 0:m.bad_id=1;expected=VB_USED;break;
        case 1:m.bad_length=1;expected=VB_USED;break;
        case 2:m.extra_used=1;expected=VB_USED;break;
        case 3:m.bad_status=1;expected=VB_STATUS;break;
        case 4:m.never_complete=1;expected=VB_TIMEOUT;break;
        case 5:m.needs_reset=1;expected=VB_STATE;break;
        default:m.config_changes=1;expected=VB_CONFIG;break;
        }
        assert(vb_read(0,out)==expected);
        assert((m.regs[0x70/4]&128)!=0 && vb_capacity()==0 && vb_reads()==0);
        assert(vb_requests()==1 && vb_flush()==VB_STATE);
        for(unsigned i=0;i<512;++i)assert(out[i]==0x7e);
    }
    negotiated();++m.generation;
    assert(vb_flush()==VB_CONFIG && vb_requests()==0);
    negotiated();m.regs[0x70/4]|=64;
    assert(vb_flush()==VB_STATE && vb_requests()==0);
}
int main(void)
{
    negotiation_refusals();sector_roundtrip_and_wrap();
    refusals_do_not_submit_or_modify_output();completion_errors_are_terminal();
    puts("virtio block MODEL PASS: negotiation, DMA bounce/queue8, wrap, sector bounds, flush, status/timeout/config failures");
    return 0;
}
