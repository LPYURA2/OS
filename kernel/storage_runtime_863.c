#include <styleos/storage_runtime_863.h>
static bool guard(struct storage_runtime_863 *s)
{
    if(s->quarantined)return false;
    struct storage_obs_863 o={0};
    if(!s->ops->observe(s->ctx,&o)||o.root!=s->root||o.epoch!=s->epoch||
       o.device!=s->device||o.sectors!=S863_DISK_SECTORS||
       !o.post_ebs||!o.single_cpu||!o.kernel_context||!o.exclusive_pio||
       !o.dma_denied||!o.disposable_data||(s->writable&&!o.flush_supported)){
        s->quarantined=true;if(!s->fault)s->fault=STORAGE863_GUARD;return false;
    }
    return true;
}
bool storage_bind_863(struct storage_runtime_863 *s,const struct storage_ops_863 *ops,
    void *ctx,uint64_t root,uint64_t epoch,uint64_t device,uint32_t first,uint32_t count,bool writable)
{
    if(!s||!ops||!ops->observe||!ops->read||(writable&&(!ops->write||!ops->flush))||
       !root||(root&4095u)||!epoch||!device||!count||first>=S863_DISK_SECTORS||
       count>S863_DISK_SECTORS-first||s->ops||s->ctx||s->root||s->epoch||
       s->device||s->first||s->count||s->writable||s->bound||s->busy||s->quarantined||s->fault)return false;
    s->ops=ops;s->ctx=ctx;s->root=root;s->epoch=epoch;s->device=device;
    s->first=first;s->count=count;s->writable=writable;s->busy=true;
    bool ok=guard(s);s->bound=ok;s->busy=false;return ok;
}
static bool enter(struct storage_runtime_863 *s,uint32_t lba)
{
    if(!s||!s->bound||s->busy||s->quarantined||lba>=s->count)return false;
    s->busy=true;
    if(!guard(s)){s->busy=false;return false;}
    return true;
}
static bool finish(struct storage_runtime_863 *s,bool ok,enum storage863_fault why)
{
    if(!ok){s->quarantined=true;if(!s->fault)s->fault=why;}
    s->busy=false;return ok;
}
static bool read_sector(struct storage_runtime_863 *s,uint32_t lba,uint8_t *temp)
{
    if(!guard(s))return false;
    bool ok=s->ops->read(s->ctx,lba,temp);
    return guard(s)&&ok;
}
bool storage_read_863(struct storage_runtime_863 *s,uint32_t lba,uint8_t out[S863_SECTOR_BYTES])
{
    if(!out||!enter(s,lba))return false;
    uint8_t temp[S863_SECTOR_BYTES];
    if(!read_sector(s,s->first+lba,temp))return finish(s,false,STORAGE863_READ);
    for(unsigned i=0;i<S863_SECTOR_BYTES;i++)out[i]=temp[i];
    return finish(s,true,STORAGE863_NONE);
}
bool storage_commit_863(struct storage_runtime_863 *s,uint32_t lba,const uint8_t in[S863_SECTOR_BYTES])
{
    if(!s||!in||!s->writable||!enter(s,lba))return false;
    uint8_t expected[S863_SECTOR_BYTES],observed[S863_SECTOR_BYTES];
    for(unsigned i=0;i<S863_SECTOR_BYTES;i++)expected[i]=in[i];
    uint32_t phys=s->first+lba;
    if(!guard(s))return finish(s,false,STORAGE863_GUARD);
    bool ok=s->ops->write(s->ctx,phys,expected);
    if(!ok){if(!s->fault)s->fault=STORAGE863_WRITE;}
    if(!guard(s)||!ok)return finish(s,false,STORAGE863_WRITE);
    ok=s->ops->flush(s->ctx);
    if(!ok){if(!s->fault)s->fault=STORAGE863_FLUSH;}
    if(!guard(s)||!ok)return finish(s,false,STORAGE863_FLUSH);
    if(!read_sector(s,phys,observed))return finish(s,false,STORAGE863_READ);
    unsigned diff=0;
    for(unsigned i=0;i<S863_SECTOR_BYTES;i++)diff|=expected[i]^observed[i];
    return finish(s,!diff,STORAGE863_COMPARE);
}
