#include <styleos/ata_runtime_863.h>
enum { BSY=128, DRDY=64, DF=32, DRQ=8, ERR=1 };
static bool fault(struct ata_runtime_863 *a,enum ata863_fault why)
{
    if(a->fault==ATA863_NONE)a->fault=why;
    return false;
}
static bool finish(struct ata_runtime_863 *a,bool ok)
{
    if(!ok)a->quarantined=true;
    else {a->phase=ATA863_IDLE;a->status_polls=0;a->last_status=0;}
    a->busy=false;
    return ok;
}
static bool sample(struct ata_runtime_863 *a,uint8_t *s)
{
    if(!a->ops->read_ops.status(a->ctx,s))return fault(a,ATA863_STATUS_IO);
    a->last_status=*s;
    if(!*s||*s==255)return fault(a,ATA863_STATUS_INVALID);
    return true;
}
static bool wait_status(struct ata_runtime_863 *a,bool data,unsigned budget)
{
    a->status_polls=0;
    for(unsigned i=0;i<budget;i++){
        ++a->status_polls;
        uint8_t s;
        if(!sample(a,&s))return false;
        if(s&BSY){
            if(budget==ATA863_COMPLETION_POLLS&&i+1<budget&&a->ops->completion_pause&&
               !a->ops->completion_pause(a->ctx))return fault(a,ATA863_PACING_IO);
            continue;
        }
        if(s&(DF|ERR))return fault(a,ATA863_STATUS_ERROR);
        if((s&DRDY)&&!!(s&DRQ)==data)return true;
    }
    return fault(a,ATA863_STATUS_TIMEOUT);
}
static bool delay(struct ata_runtime_863 *a)
{
    for(unsigned i=0;i<4;i++){
        uint8_t s;
        if(!sample(a,&s))return false;
    }
    return true;
}
/* Extended transport only permits the fixed write/flush commands in these
 * paths; the existing read-only S857 command allowlist remains unchanged. */
static bool reg(struct ata_runtime_863 *a,unsigned r,uint8_t v)
{return a->ops->read_ops.write_reg(a->ctx,r,v)||fault(a,ATA863_REGISTER_IO);}
static bool bind_identity(struct ata_runtime_863 *a,const struct ata_runtime_ops_863 *ops,void *ctx,const char *serial)
{
    if(!a||!ops||!ops->read_ops.status||!ops->read_ops.write_reg||
       !ops->read_ops.read_data||!ops->write_data||a->ops||a->ctx||a->bound||
       a->busy||a->quarantined||a->flush_supported||a->pending_flush||
       a->fault||a->phase||a->status_polls||a->last_status||
       a->reader.identified||a->reader.busy||a->reader.quarantined)return false;
    a->ops=ops;a->ctx=ctx;a->busy=true;a->phase=ATA863_BIND;
    uint16_t w[ATA857_WORDS];
    if(!ata857_identify(&a->reader,&ops->read_ops,ctx,w)||(serial&&!ata864_serial_match(w,serial)))return finish(a,fault(a,ATA863_IDENTIFY));
    /* IDENTIFY word 83 validity + FLUSH CACHE. No unsupported device writes. */
    if((w[83]&0xc000u)!=0x4000u||!(w[83]&0x1000u))return finish(a,fault(a,ATA863_FLUSH_UNSUPPORTED));
    a->flush_supported=true;a->bound=true;return finish(a,true);
}
bool ata_runtime_bind_863(struct ata_runtime_863 *a,const struct ata_runtime_ops_863 *ops,void *ctx)
{return bind_identity(a,ops,ctx,0);}
bool ata_runtime_bind_serial_864(struct ata_runtime_863 *a,const struct ata_runtime_ops_863 *ops,void *ctx,const char serial[20])
{return serial&&bind_identity(a,ops,ctx,serial);}
static bool enter(struct ata_runtime_863 *a)
{
    if(!a||!a->bound||a->busy||a->quarantined||!a->flush_supported||
       !a->reader.identified||a->reader.quarantined)return false;
    a->busy=true;return true;
}
static bool read_sector(void *ctx,uint32_t lba,uint8_t out[512],bool (*pace)(void *))
{
    struct ata_runtime_863 *a=ctx;
    if(!a||!out||lba>=ATA857_SECTORS||a->pending_flush||!enter(a))return false;
    uint16_t w[ATA857_WORDS];
    a->phase=ATA863_READ;
    if(!(pace?ata857_read_paced(&a->reader,&a->ops->read_ops,a->ctx,lba,w,pace):ata857_read(&a->reader,&a->ops->read_ops,a->ctx,lba,w)))return finish(a,fault(a,ATA863_READ_TRANSFER));
    for(unsigned i=0;i<ATA857_WORDS;i++){out[2*i]=(uint8_t)w[i];out[2*i+1]=(uint8_t)(w[i]>>8);}
    return finish(a,true);
}
bool ata_runtime_read_863(void *ctx,uint32_t lba,uint8_t out[512])
{return read_sector(ctx,lba,out,0);}
bool ata_runtime_read_paced_863(void *ctx,uint32_t lba,uint8_t out[512],bool (*pace)(void *))
{return pace&&read_sector(ctx,lba,out,pace);}
bool ata_runtime_write_863(void *ctx,uint32_t lba,const uint8_t in[512])
{
    struct ata_runtime_863 *a=ctx;
    if(!a||!in||lba>=ATA857_SECTORS||a->pending_flush||!enter(a))return false;
    a->phase=ATA863_WRITE_SETUP;
    if(!wait_status(a,false,ATA857_POLLS)||!reg(a,8,2)||!reg(a,6,0xe0)||!delay(a)||
       !wait_status(a,false,ATA857_POLLS)||!reg(a,1,0)||!reg(a,2,1)||!reg(a,3,(uint8_t)lba)||
       !reg(a,4,(uint8_t)(lba>>8))||!reg(a,5,0)||!reg(a,7,0x30)||
       !delay(a)||!wait_status(a,true,ATA857_POLLS))return finish(a,false);
    a->phase=ATA863_WRITE_DATA;
    for(unsigned i=0;i<ATA857_WORDS;i++)
        if(!a->ops->write_data(a->ctx,(uint16_t)(in[2*i]|((uint16_t)in[2*i+1]<<8))))return finish(a,fault(a,ATA863_DATA_IO));
    a->phase=ATA863_WRITE_COMPLETE;
    if(!delay(a)||!wait_status(a,false,ATA863_COMPLETION_POLLS))return finish(a,false);
    a->pending_flush=true;return finish(a,true);
}
bool ata_runtime_flush_863(void *ctx)
{
    struct ata_runtime_863 *a=ctx;
    if(!a||!a->pending_flush||!enter(a))return false;
    a->phase=ATA863_FLUSH;
    if(!wait_status(a,false,ATA857_POLLS)||!reg(a,6,0xe0)||!delay(a)||!wait_status(a,false,ATA857_POLLS)||
       !reg(a,7,0xe7)||!delay(a)||!wait_status(a,false,ATA863_COMPLETION_POLLS))return finish(a,false);
    a->pending_flush=false;return finish(a,true);
}
