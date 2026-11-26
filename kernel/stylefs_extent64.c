#include <styleos/stylefs_extent64.h>

uint64_t sfs64_sectors(uint64_t bytes)
{return bytes/BS_SECTOR_BYTES+(bytes%BS_SECTOR_BYTES!=0);}

bool sfs64_extent_fits(struct sfs_extent64 e,uint64_t capacity)
{
    /* Subtraction, not first+count; ceil division never overflows. Empty
     * extent has canonical first=0 and needs no storage. */
    if(!e.bytes)return e.first==0;
    return e.first<capacity&&sfs64_sectors(e.bytes)<=capacity-e.first;
}
bool sfs64_window(struct sfs_extent64 e,uint64_t capacity,uint64_t offset,
                  uint64_t remaining,struct sfs_window64 *out)
{
    if(!out||!remaining||!sfs64_extent_fits(e,capacity)||
       offset>=e.bytes||remaining>e.bytes-offset)return false;
    uint64_t sector=e.first+offset/BS_SECTOR_BYTES;
    unsigned within=(unsigned)(offset%BS_SECTOR_BYTES);
    unsigned n=BS_SECTOR_BYTES-within;
    if(remaining<n)n=(unsigned)remaining;
    *out=(struct sfs_window64){sector,within,n};
    return true;
}
static bool span(const void *p,size_t n)
{return p&&n<=UINTPTR_MAX-(uintptr_t)p;}
static bool disjoint(const void *p,size_t n,const void *q,size_t m)
{
    if(!n||!m)return true;
    return span(p,n)&&span(q,m)&&
        ((uintptr_t)p+n<=(uintptr_t)q||(uintptr_t)q+m<=(uintptr_t)p);
}
static enum bs_status grant_length(struct bs_service *s,uint64_t owner,uint64_t token,
                                   unsigned right,uint64_t *capacity)
{
    if(!s||!owner||!token)return BS_INVALID;
    if(s->busy)return BS_BUSY;
    for(unsigned i=0;i<BS_GRANTS;++i){
        const struct bs_grant *g=&s->grants[i];
        if(g->token!=token)continue;
        if(g->owner!=owner||!(g->rights&right))return BS_DENIED;
        if(g->device>=BS_DEVICES)return BS_STALE;
        const struct bs_device *d=&s->devices[g->device];
        if(!g->generation||d->generation!=g->generation)return BS_STALE;
        if(d->recovery)return BS_RECOVERY;
        if(!g->length||g->start>=d->backend.sectors||
           g->length>d->backend.sectors-g->start)return BS_RANGE;
        if(right==BS_WRITE&&(!d->backend.writable||d->backend.system_media||
                             !d->backend.write||!d->backend.flush))return BS_DENIED;
        *capacity=g->length;return BS_OK;
    }
    return BS_STALE;
}
static enum bs_status preflight(struct bs_service *s,uint64_t owner,uint64_t token,
    struct sfs_extent64 e,uint64_t offset,const void *buffer,size_t length,size_t *completed,
    unsigned right,uint64_t *capacity)
{
    if(!s||!span(completed,sizeof *completed)||
       !disjoint(completed,sizeof *completed,s,sizeof *s)||
       (length&&(!span(buffer,length)||!disjoint(buffer,length,s,sizeof *s)||
                 !disjoint(buffer,length,completed,sizeof *completed))))return BS_INVALID;
    *completed=0;
    enum bs_status r=grant_length(s,owner,token,right,capacity);
    if(r!=BS_OK)return r;
    if(!sfs64_extent_fits(e,*capacity)||offset>e.bytes||length>e.bytes-offset||
       length>SFS64_WINDOW_BYTES)return BS_RANGE;
    /* Count actual touched sectors, including an unaligned first fragment. */
    if(length&&sfs64_sectors((uint64_t)length+offset%BS_SECTOR_BYTES)>SFS64_WINDOW_SECTORS)
        return BS_RANGE;
    if(right==BS_WRITE&&length&&(offset%BS_SECTOR_BYTES||
       (length%BS_SECTOR_BYTES&&length!=e.bytes-offset)))return BS_RANGE;
    return BS_OK;
}
enum bs_status sfs64_read_window(struct bs_service *s,uint64_t owner,uint64_t token,
    struct sfs_extent64 e,uint64_t offset,uint8_t *out,size_t length,size_t *completed)
{
    uint64_t capacity=0;
    enum bs_status r=preflight(s,owner,token,e,offset,out,length,completed,BS_READ,&capacity);
    if(r!=BS_OK)return r;
    uint8_t sector[BS_SECTOR_BYTES];
    while(*completed<length){
        struct sfs_window64 w;
        if(!sfs64_window(e,capacity,offset+*completed,length-*completed,&w))return BS_RANGE;
        r=bs_read_sector(s,owner,token,w.sector,sector);
        if(r!=BS_OK)return r;
        for(unsigned i=0;i<w.bytes;++i)out[*completed+i]=sector[w.offset+i];
        *completed+=w.bytes;
    }
    return BS_OK;
}
enum bs_status sfs64_stage_window(struct bs_service *s,uint64_t owner,uint64_t token,
    struct sfs_extent64 e,uint64_t offset,const uint8_t *in,size_t length,size_t *completed)
{
    uint64_t capacity=0;
    enum bs_status r=preflight(s,owner,token,e,offset,in,length,completed,BS_WRITE,&capacity);
    if(r!=BS_OK)return r;
    uint8_t sector[BS_SECTOR_BYTES];
    while(*completed<length){
        struct sfs_window64 w;
        if(!sfs64_window(e,capacity,offset+*completed,length-*completed,&w)||w.offset)return BS_RANGE;
        for(unsigned i=0;i<BS_SECTOR_BYTES;++i)sector[i]=i<w.bytes?in[*completed+i]:0;
        r=bs_write_sector(s,owner,token,w.sector,sector);
        if(r!=BS_OK)return r;
        *completed+=w.bytes;
    }
    return BS_OK;
}
