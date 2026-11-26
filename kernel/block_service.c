#include <styleos/block_service.h>
#include <stddef.h>

static void copy_bytes(void *dst,const void *src,size_t n)
{
    uint8_t *d=dst;const uint8_t *s=src;
    for(size_t i=0;i<n;++i)d[i]=s[i];
}
static void wipe(struct bs_service *s)
{
    volatile uint8_t *p=s->staging;
    for(unsigned i=0;i<BS_SECTOR_BYTES;++i)p[i]=0;
}
/* Reject overlap with service-owned state even for a trusted caller. The
 * eventual copy-in/out broker separately proves actual mapped kernel spans. */
static bool external(const struct bs_service *s,const void *p,size_t bytes)
{
    if(!p||!bytes)return false;
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)s;
    if(bytes>UINTPTR_MAX-a||sizeof *s>UINTPTR_MAX-b)return false;
    return a+bytes<=b||b+sizeof *s<=a;
}
static struct bs_device *device(struct bs_service *s,uint64_t gen)
{
    if(!gen)return 0;
    for(unsigned i=0;i<BS_DEVICES;++i)
        if(s->devices[i].generation==gen)return &s->devices[i];
    return 0;
}
static struct bs_grant *grant(struct bs_service *s,uint64_t token)
{
    if(!token)return 0;
    for(unsigned i=0;i<BS_GRANTS;++i)
        if(s->grants[i].token==token)return &s->grants[i];
    return 0;
}
enum bs_status bs_attach(struct bs_service *s,const struct bs_backend *b,uint64_t *gen)
{
    if(!s||!external(s,b,sizeof *b)||!external(s,gen,sizeof *gen))return BS_INVALID;
    if(s->busy)return BS_BUSY;
    if(!b->identity||!b->sectors||!b->read||
       (b->writable&&(!b->write||!b->flush||b->system_media)))return BS_INVALID;
    unsigned free_slot=BS_DEVICES;
    for(unsigned i=0;i<BS_DEVICES;++i){
        const struct bs_device *d=&s->devices[i];
        if(d->generation&&d->backend.identity==b->identity)return BS_DENIED;
        if(!d->generation&&free_slot==BS_DEVICES)free_slot=i;
    }
    if(free_slot==BS_DEVICES||s->serial==UINT64_MAX)return BS_FULL;
    struct bs_backend backend=*b;
    if(!backend.writable){backend.write=0;backend.flush=0;}
    s->devices[free_slot]=(struct bs_device){.backend=backend,.generation=++s->serial};
    *gen=s->serial;return BS_OK;
}
enum bs_status bs_detach(struct bs_service *s,uint64_t gen)
{
    if(!s||!gen)return BS_INVALID;
    if(s->busy)return BS_BUSY;
    struct bs_device *d=device(s,gen);
    if(!d)return BS_STALE;
    for(unsigned i=0;i<BS_GRANTS;++i)
        if(s->grants[i].generation==gen)s->grants[i]=(struct bs_grant){0};
    *d=(struct bs_device){0};
    return BS_OK;
}
enum bs_status bs_grant_range(struct bs_service *s,uint64_t gen,uint64_t owner,
                             uint64_t start,uint64_t length,unsigned rights,uint64_t *token)
{
    if(!s||!owner||!gen||!external(s,token,sizeof *token)||!length||
       !rights||(rights&~(BS_READ|BS_WRITE)))return BS_INVALID;
    if(s->busy)return BS_BUSY;
    struct bs_device *d=device(s,gen);
    if(!d)return BS_STALE;
    if(d->recovery)return BS_RECOVERY;
    if((rights&BS_WRITE)&&!d->backend.writable)return BS_DENIED;
    if(start>=d->backend.sectors||length>d->backend.sectors-start)return BS_RANGE;
    unsigned free_slot=BS_GRANTS;
    for(unsigned i=0;i<BS_GRANTS;++i){
        const struct bs_grant *g=&s->grants[i];
        if(!g->token){if(free_slot==BS_GRANTS)free_slot=i;continue;}
        if(g->generation==gen&&((g->rights|rights)&BS_WRITE)&&
           start<g->start+g->length&&g->start<start+length)return BS_DENIED;
    }
    if(free_slot==BS_GRANTS||s->serial==UINT64_MAX)return BS_FULL;
    s->grants[free_slot]=(struct bs_grant){++s->serial,owner,gen,start,length,
                                          (unsigned)(d-s->devices),rights};
    *token=s->serial;return BS_OK;
}
enum bs_status bs_revoke(struct bs_service *s,uint64_t owner,uint64_t token)
{
    if(!s||!owner||!token)return BS_INVALID;
    if(s->busy)return BS_BUSY;
    struct bs_grant *g=grant(s,token);
    if(!g)return BS_STALE;
    if(g->owner!=owner)return BS_DENIED;
    *g=(struct bs_grant){0};return BS_OK;
}
enum bs_status bs_revoke_owner(struct bs_service *s,uint64_t owner)
{
    if(!s||!owner)return BS_INVALID;
    if(s->busy)return BS_BUSY;
    for(unsigned i=0;i<BS_GRANTS;++i)
        if(s->grants[i].owner==owner)s->grants[i]=(struct bs_grant){0};
    return BS_OK;
}
static enum bs_status access(struct bs_service *s,uint64_t owner,uint64_t token,
                             uint64_t sector,const void *buffer,unsigned right,
                             struct bs_device **out,uint64_t *lba)
{
    if(!s||!owner||!token||!external(s,buffer,BS_SECTOR_BYTES))return BS_INVALID;
    if(s->busy)return BS_BUSY;
    const struct bs_grant *g=grant(s,token);
    if(!g)return BS_STALE;
    if(g->owner!=owner||!(g->rights&right))return BS_DENIED;
    if(g->device>=BS_DEVICES)return BS_STALE;
    struct bs_device *d=&s->devices[g->device];
    if(d->generation!=g->generation)return BS_STALE;
    if(d->recovery)return BS_RECOVERY;
    if(sector>=g->length||g->start>=d->backend.sectors||
       sector>=d->backend.sectors-g->start)return BS_RANGE;
    if(right==BS_WRITE&&(!d->backend.writable||d->backend.system_media||
                         !d->backend.write||!d->backend.flush))return BS_DENIED;
    *out=d;*lba=g->start+sector;
    return BS_OK;
}
enum bs_status bs_read_sector(struct bs_service *s,uint64_t owner,uint64_t token,
                              uint64_t sector,uint8_t out[BS_SECTOR_BYTES])
{
    struct bs_device *d;uint64_t lba;
    enum bs_status r=access(s,owner,token,sector,out,BS_READ,&d,&lba);
    if(r!=BS_OK)return r;
    s->busy=true;wipe(s);
    if(d->backend.read(d->backend.context,lba,s->staging))
        copy_bytes(out,s->staging,BS_SECTOR_BYTES);
    else {d->recovery=true;r=BS_IO;}
    wipe(s);s->busy=false;return r;
}
enum bs_status bs_write_sector(struct bs_service *s,uint64_t owner,uint64_t token,
                               uint64_t sector,const uint8_t in[BS_SECTOR_BYTES])
{
    struct bs_device *d;uint64_t lba;
    enum bs_status r=access(s,owner,token,sector,in,BS_WRITE,&d,&lba);
    if(r!=BS_OK)return r;
    s->busy=true;copy_bytes(s->staging,in,BS_SECTOR_BYTES);
    if(!d->backend.write(d->backend.context,lba,s->staging)||
       !d->backend.flush(d->backend.context)){
        d->recovery=true;r=BS_UNCERTAIN;
    }
    wipe(s);s->busy=false;return r;
}

enum bs_status bs_write_staged_batch(struct bs_service *s,uint64_t owner,uint64_t token,
                                      uint64_t sector,const uint8_t *in,size_t n)
{
    if(!s||!external(s,in,n)||n>64u*BS_SECTOR_BYTES)return BS_INVALID;
    uint64_t count=n/BS_SECTOR_BYTES+(n%BS_SECTOR_BYTES!=0);
    if(!count||count-1>UINT64_MAX-sector)return BS_RANGE;
    struct bs_device *d,*last;uint64_t lba,end;
    uint8_t probe[BS_SECTOR_BYTES];
    enum bs_status r=access(s,owner,token,sector,probe,BS_WRITE,&d,&lba);
    if(r!=BS_OK)return r;
    r=access(s,owner,token,sector+count-1,probe,BS_WRITE,&last,&end);
    if(r!=BS_OK)return r;
    if(d!=last||end-lba!=count-1)return BS_RANGE;
    s->busy=true;
    for(uint64_t i=0;i<count;++i){
        size_t at=(size_t)i*BS_SECTOR_BYTES;
        size_t used=n-at<BS_SECTOR_BYTES?n-at:BS_SECTOR_BYTES;
        wipe(s);copy_bytes(s->staging,in+at,used);
        if(!d->backend.write(d->backend.context,lba+i,s->staging)){
            d->recovery=true;r=BS_UNCERTAIN;break;
        }
    }
    if(r==BS_OK&&!d->backend.flush(d->backend.context)){
        d->recovery=true;r=BS_UNCERTAIN;
    }
    wipe(s);s->busy=false;return r;
}
