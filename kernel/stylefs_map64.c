#include <styleos/stylefs_map64.h>

static const uint8_t magic[8]={'S','F','6','4','M','A','P','1'};
static uint32_t get32(const uint8_t *p)
{uint32_t n=0;for(unsigned i=0;i<4;++i)n|=(uint32_t)p[i]<<(i*8);return n;}
static uint64_t get64(const uint8_t *p)
{uint64_t n=0;for(unsigned i=0;i<8;++i)n|=(uint64_t)p[i]<<(i*8);return n;}
static void put32(uint8_t *p,uint32_t n)
{for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(i*8));}
static void put64(uint8_t *p,uint64_t n)
{for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(n>>(i*8));}
static uint32_t digest(const uint8_t *p,size_t n)
{
    uint32_t c=UINT32_MAX;
    for(size_t i=0;i<n;++i){
        c^=i>=56&&i<60?0:p[i];
        for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);
    }
    return ~c;
}
size_t sfs64_map_length(size_t count)
{
    if(count>UINT32_MAX||count>(SIZE_MAX-SFS64_MAP_HEADER)/SFS64_MAP_RECORD)return 0;
    return SFS64_MAP_HEADER+count*SFS64_MAP_RECORD;
}
static struct sfs64_run run_at(const void *records,size_t i,bool encoded)
{
    if(!encoded)return ((const struct sfs64_run *)records)[i];
    const uint8_t *p=(const uint8_t *)records+i*SFS64_MAP_RECORD;
    return (struct sfs64_run){get64(p),get64(p+8)};
}
static bool valid(struct sfs64_file file,uint64_t capacity,uint64_t data_start,
                  const void *records,size_t count,bool encoded)
{
    if(!file.id||!file.revision||!data_start||data_start>=capacity||
       !sfs64_map_length(count)||(count&&!records))return false;
    if(!file.bytes)return !count&&!file.checksum;
    if(!count)return false;
    uint64_t left=sfs64_sectors(file.bytes);
    /* Caller supplies bounded metadata bytes. Reject overlap even when
     * logical run order differs from physical order; no dynamic allocation. */
    for(size_t i=0;i<count;++i){
        struct sfs64_run a=run_at(records,i,encoded);
        if(!a.sectors||a.first<data_start||a.first>=capacity||
           a.sectors>capacity-a.first||a.sectors>left)return false;
        for(size_t j=0;j<i;++j){
            struct sfs64_run b=run_at(records,j,encoded);
            if(a.first<b.first+b.sectors&&b.first<a.first+a.sectors)return false;
        }
        left-=a.sectors;
    }
    return left==0;
}
bool sfs64_map_encode(uint8_t *out,size_t n,struct sfs64_file file,uint64_t capacity,
                      uint64_t data_start,const struct sfs64_run *runs,size_t count)
{
    if(!out||n!=sfs64_map_length(count)||!n||
       !valid(file,capacity,data_start,runs,count,false))return false;
    for(unsigned i=0;i<SFS64_MAP_HEADER;++i)out[i]=0;
    for(unsigned i=0;i<8;++i)out[i]=magic[i];
    put64(out+8,file.bytes);put64(out+16,capacity);put64(out+24,data_start);
    put64(out+32,file.id);put64(out+40,file.revision);
    put32(out+48,(uint32_t)count);put32(out+52,file.checksum);
    for(size_t i=0;i<count;++i){
        uint8_t *p=out+SFS64_MAP_HEADER+i*SFS64_MAP_RECORD;
        put64(p,runs[i].first);put64(p+8,runs[i].sectors);
    }
    put32(out+56,digest(out,n));return true;
}
bool sfs64_map_decode(const uint8_t *in,size_t n,uint64_t capacity,uint64_t data_start,
                      struct sfs64_map *out)
{
    if(!in||!out||n<SFS64_MAP_HEADER)return false;
    for(unsigned i=0;i<8;++i)if(in[i]!=magic[i])return false;
    uint32_t count=get32(in+48);
    if(n!=sfs64_map_length(count)||get64(in+16)!=capacity||
       get64(in+24)!=data_start||get32(in+60)||get32(in+56)!=digest(in,n))return false;
    struct sfs64_file file={get64(in+32),get64(in+40),get64(in+8),get32(in+52)};
    const uint8_t *records=in+SFS64_MAP_HEADER;
    if(!valid(file,capacity,data_start,records,count,true))return false;
    *out=(struct sfs64_map){file,capacity,data_start,count,records};return true;
}
bool sfs64_map_chunk(const struct sfs64_map *m,uint64_t offset,size_t wanted,
                     struct sfs_extent64 *extent,uint64_t *local,size_t *n)
{
    if(!m||!extent||!local||!n||!wanted||offset>=m->file.bytes||
       wanted>m->file.bytes-offset||!m->records)return false;
    uint64_t left=m->file.bytes;
    for(size_t i=0;i<m->count;++i){
        struct sfs64_run r=run_at(m->records,i,true);
        if(!r.sectors||!left)return false;
        /* Multiplication occurs only when its result is below left.
         * Handles the last partial sector even for UINT64_MAX file size. */
        uint64_t bytes=r.sectors>=sfs64_sectors(left)?left:r.sectors*BS_SECTOR_BYTES;
        if(offset<bytes){
            uint64_t avail=bytes-offset;
            size_t take=avail<wanted?(size_t)avail:wanted;
            size_t budget=SFS64_WINDOW_BYTES-(size_t)(offset%BS_SECTOR_BYTES);
            if(take>budget)take=budget;
            struct sfs_extent64 e={r.first,bytes};
            if(!take||!sfs64_extent_fits(e,m->capacity)||r.first<m->data_start)return false;
            *extent=e;*local=offset;*n=take;return true;
        }
        offset-=bytes;left-=bytes;
    }
    return false;
}
