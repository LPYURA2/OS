#include <styleos/ata_runtime_48.h>
enum { BSY=128, DRDY=64, DF=32, DRQ=8, ERR=1 };

bool ata48_geometry(const uint16_t w[256],uint64_t expected)
{
    if(!w||!expected||expected>ATA48_SECTORS||!w[0]||w[0]==UINT16_MAX||
       (w[0]&0x8000u)||!(w[49]&0x200u)||
       (w[83]&0xc000u)!=0x4000u||!(w[83]&0x0400u)||!(w[83]&0x2000u)||
       (w[106]&0xd000u)==0x5000u)return false;
    uint64_t sectors=0;
    for(unsigned i=0;i<4;++i)sectors|=(uint64_t)w[100+i]<<(16*i);
    return sectors==expected;
}
static bool sample(struct ata_runtime_48 *a,uint8_t *s)
{return a->ops->read_ops.status(a->context,s)&&*s&&*s!=255;}
static bool wait_status(struct ata_runtime_48 *a,bool data,unsigned budget)
{
    for(unsigned i=0;i<budget;++i){
        uint8_t s;
        if(!sample(a,&s))return false;
        if(s&BSY){
            if(budget==ATA863_COMPLETION_POLLS&&i+1<budget&&a->ops->completion_pause&&
               !a->ops->completion_pause(a->context))return false;
            continue;
        }
        if(s&(DF|ERR))return false;
        if((s&DRDY)&&!!(s&DRQ)==data)return true;
    }
    return false;
}
static bool delay(struct ata_runtime_48 *a)
{
    for(unsigned i=0;i<4;++i){uint8_t s;if(!sample(a,&s))return false;}
    return true;
}
static bool reg(struct ata_runtime_48 *a,unsigned r,uint8_t v)
{return a->ops->read_ops.write_reg(a->context,r,v);}
static bool finish(struct ata_runtime_48 *a,bool ok)
{a->busy=false;if(!ok)a->quarantined=true;return ok;}
static bool select(struct ata_runtime_48 *a,uint8_t drive)
{
    return wait_status(a,false,ATA857_POLLS)&&reg(a,8,2)&&reg(a,6,drive)&&
        delay(a)&&wait_status(a,false,ATA857_POLLS);
}
bool ata48_bind(struct ata_runtime_48 *a,const struct ata_runtime_ops_863 *ops,
                void *ctx,const char serial[20],uint64_t expected)
{
    if(!a||!ops||!serial||!expected||expected>ATA48_SECTORS||
       !ops->read_ops.status||!ops->read_ops.write_reg||!ops->read_ops.read_data||
       !ops->write_data||a->ops||a->context||a->sectors||a->busy||a->bound||
       a->quarantined||a->pending_flush)return false;
    a->ops=ops;a->context=ctx;a->busy=true;
    uint16_t w[256];
    if(!select(a,0xa0)||!reg(a,1,0)||!reg(a,2,0)||!reg(a,3,0)||!reg(a,4,0)||
       !reg(a,5,0)||!reg(a,7,0xec)||!delay(a)||!wait_status(a,true,ATA857_POLLS))
        return finish(a,false);
    for(unsigned i=0;i<256;++i)
        if(!ops->read_ops.read_data(ctx,&w[i]))return finish(a,false);
    if(!delay(a)||!wait_status(a,false,ATA857_POLLS)||
       !ata48_geometry(w,expected)||!ata864_serial_match(w,serial))return finish(a,false);
    a->sectors=expected;a->bound=true;return finish(a,true);
}
static bool ready(const struct ata_runtime_48 *a)
{return a&&a->bound&&!a->busy&&!a->quarantined&&a->sectors&&a->sectors<=ATA48_SECTORS;}
static bool setup(struct ata_runtime_48 *a,uint64_t lba,uint8_t command)
{
    /* High-order task-file bytes FIRST, then low-order. Count exactly one.
     * No drive-head address nibble; use the LBA bit for the master. */
    return select(a,0xe0)&&
        reg(a,1,0)&&reg(a,2,0)&&reg(a,3,(uint8_t)(lba>>24))&&
        reg(a,4,(uint8_t)(lba>>32))&&reg(a,5,(uint8_t)(lba>>40))&&
        reg(a,1,0)&&reg(a,2,1)&&reg(a,3,(uint8_t)lba)&&
        reg(a,4,(uint8_t)(lba>>8))&&reg(a,5,(uint8_t)(lba>>16))&&
        reg(a,7,command)&&delay(a)&&wait_status(a,true,ATA857_POLLS);
}
bool ata48_read(struct ata_runtime_48 *a,uint64_t lba,uint8_t out[512])
{
    if(!ready(a)||!out||a->pending_flush||lba>=a->sectors)return false;
    a->busy=true;uint16_t temp[256];
    if(!setup(a,lba,0x24))return finish(a,false);
    for(unsigned i=0;i<256;++i)
        if(!a->ops->read_ops.read_data(a->context,&temp[i]))return finish(a,false);
    if(!delay(a)||!wait_status(a,false,ATA857_POLLS))return finish(a,false);
    for(unsigned i=0;i<256;++i){out[2*i]=(uint8_t)temp[i];out[2*i+1]=(uint8_t)(temp[i]>>8);}
    return finish(a,true);
}
bool ata48_write(struct ata_runtime_48 *a,uint64_t lba,const uint8_t in[512])
{
    if(!ready(a)||!in||a->pending_flush||lba>=a->sectors)return false;
    a->busy=true;
    if(!setup(a,lba,0x34))return finish(a,false);
    for(unsigned i=0;i<256;++i)
        if(!a->ops->write_data(a->context,(uint16_t)(in[2*i]|((uint16_t)in[2*i+1]<<8))))
            return finish(a,false);
    if(!delay(a)||!wait_status(a,false,ATA863_COMPLETION_POLLS))return finish(a,false);
    a->pending_flush=true;return finish(a,true);
}
bool ata48_flush(struct ata_runtime_48 *a)
{
    if(!ready(a)||!a->pending_flush)return false;
    a->busy=true;
    if(!select(a,0xe0)||!reg(a,7,0xea)||!delay(a)||
       !wait_status(a,false,ATA863_COMPLETION_POLLS))return finish(a,false);
    a->pending_flush=false;return finish(a,true);
}
