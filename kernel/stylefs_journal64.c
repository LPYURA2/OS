#include <styleos/stylefs_journal64.h>

static const uint8_t super_magic[8]={'S','F','6','4','J','N','L','1'};
static const uint8_t commit_magic[8]={'S','F','6','4','C','M','T','1'};
static void zero(uint8_t *p,size_t n){for(size_t i=0;i<n;++i)p[i]=0;}
static void copy(uint8_t *p,const uint8_t *q,size_t n){for(size_t i=0;i<n;++i)p[i]=q[i];}
static bool equal(const uint8_t *p,const uint8_t *q,size_t n){for(size_t i=0;i<n;++i)if(p[i]!=q[i])return false;return true;}
static bool zeros(const uint8_t *p,size_t n){for(size_t i=0;i<n;++i)if(p[i])return false;return true;}
static uint32_t get32(const uint8_t *p){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t get64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;++i)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
static void put64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p,size_t n){
    uint32_t c=UINT32_MAX;
    for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
    return ~c;
}
static bool geometry(uint64_t cap,uint64_t start){return start>=SFS64_JOURNAL_SECTORS&&start<cap;}
static bool next(const uint8_t *p,size_t n,size_t *at,uint64_t cap,uint64_t start,struct sfs64_map *m)
{
    if(*at>n||n-*at<4)return false;
    uint32_t bytes=get32(p+*at);*at+=4;
    if(bytes>n-*at||!sfs64_map_decode(p+*at,bytes,cap,start,m))return false;
    *at+=bytes;return true;
}
static bool overlap(const struct sfs64_map *a,const struct sfs64_map *b)
{
    for(size_t i=0;i<a->count;++i){
        const uint8_t *p=a->records+i*SFS64_MAP_RECORD;
        uint64_t lo=get64(p),hi=lo+get64(p+8);
        for(size_t j=0;j<b->count;++j){
            const uint8_t *q=b->records+j*SFS64_MAP_RECORD;
            uint64_t l=get64(q),h=l+get64(q+8);
            if(lo<h&&l<hi)return true;
        }
    }
    return false;
}
bool sfs64_snapshot_valid(const uint8_t *p,size_t n,uint64_t cap,uint64_t start)
{
    if(!geometry(cap,start)||n>SFS64_BANK_BYTES||(n&&!p))return false;
    size_t at=0;
    while(at<n){
        size_t before=at;struct sfs64_map m;
        if(!next(p,n,&at,cap,start,&m))return false;
        for(size_t prev=0;prev<before;){
            struct sfs64_map old;
            if(!next(p,before,&prev,cap,start,&old)||m.file.id==old.file.id||overlap(&m,&old))return false;
        }
    }
    return true;
}
static uint64_t header_lba(unsigned bank){return 1+(uint64_t)bank*(1+SFS64_BANK_SECTORS);}
static void super(uint8_t p[512],uint64_t cap,uint64_t start)
{
    zero(p,512);copy(p,super_magic,8);put64(p+8,cap);put64(p+16,start);
    put32(p+24,SFS64_BANK_SECTORS);put32(p+28,crc(p,28));
}
static void header(uint8_t p[512],unsigned bank,uint64_t gen,size_t n,uint32_t sum,uint64_t cap,uint64_t start)
{
    zero(p,512);copy(p,commit_magic,8);put64(p+8,gen);
    put32(p+16,bank);put32(p+20,(uint32_t)n);put32(p+24,sum);
    put64(p+28,cap);put64(p+36,start);put32(p+44,crc(p,44));
}
static bool header_valid(const uint8_t p[512],unsigned bank,uint64_t cap,uint64_t start)
{
    return equal(p,commit_magic,8)&&get64(p+8)&&get32(p+16)==bank&&
        get32(p+20)<=SFS64_BANK_BYTES&&get64(p+28)==cap&&get64(p+36)==start&&
        get32(p+44)==crc(p,44)&&zeros(p+48,464);
}
static bool grant(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t cap,bool write)
{
    if(!b||b->busy||!owner||!token)return false;
    for(unsigned i=0;i<BS_GRANTS;++i){
        const struct bs_grant *g=&b->grants[i];
        if(g->token!=token)continue;
        if(g->owner!=owner||g->length!=cap||g->device>=BS_DEVICES||
           !(g->rights&BS_READ)||(write&&!(g->rights&BS_WRITE)))return false;
        const struct bs_device *d=&b->devices[g->device];
        return g->generation&&d->generation==g->generation&&!d->recovery&&
            (!write||(d->backend.writable&&!d->backend.system_media));
    }
    return false;
}
bool sfs64_journal_ready(const struct sfs64_journal *j,bool write)
{return j&&j->valid&&j->active<2&&grant(j->block,j->owner,j->token,j->capacity,write);}
static enum sj_status put(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t lba,const uint8_t p[512])
{
    uint8_t check[512];
    if(bs_write_sector(b,owner,token,lba,p)!=BS_OK)return SJ_UNCERTAIN;
    if(bs_read_sector(b,owner,token,lba,check)!=BS_OK||!equal(p,check,512))return SJ_UNCERTAIN;
    return SJ_OK;
}
enum sj_status sfs64_journal_mount(struct sfs64_journal *j,struct bs_service *b,
                                  uint64_t owner,uint64_t token,uint64_t cap,uint64_t start)
{
    if(!j)return SJ_INVALID;
    j->valid=false;j->writer=0;
    if(!geometry(cap,start)||!grant(b,owner,token,cap,false))return SJ_STALE;
    uint8_t p[512],expected[512];super(expected,cap,start);
    if(bs_read_sector(b,owner,token,0,p)!=BS_OK)return SJ_IO;
    if(!equal(p,expected,512))return SJ_SIGNATURE;
    for(unsigned bank=0;bank<2;++bank){
        j->generation[bank]=0;j->length[bank]=0;
        if(bs_read_sector(b,owner,token,header_lba(bank),p)!=BS_OK)return SJ_IO;
        if(!header_valid(p,bank,cap,start))continue;
        size_t n=get32(p+20);uint64_t gen=get64(p+8);uint32_t sum=get32(p+24);
        uint8_t *data=j->payload[bank];
        for(unsigned i=0;i<SFS64_BANK_SECTORS;++i)
            if(bs_read_sector(b,owner,token,header_lba(bank)+1+i,data+512*i)!=BS_OK)return SJ_IO;
        if(!zeros(data+n,SFS64_BANK_BYTES-n)||crc(data,n)!=sum||
           !sfs64_snapshot_valid(data,n,cap,start))continue;
        j->generation[bank]=gen;j->length[bank]=n;
    }
    if(j->generation[0]==j->generation[1])return SJ_CORRUPT;
    j->active=j->generation[1]>j->generation[0];
    j->block=b;j->owner=owner;j->token=token;j->capacity=cap;j->data_start=start;j->valid=true;
    return SJ_OK;
}
enum sj_status sfs64_journal_publish(struct sfs64_journal *j,const uint8_t *p,size_t n)
{
    if(!j||!j->valid||j->active>1||j->writer)return SJ_STALE;
    if(!grant(j->block,j->owner,j->token,j->capacity,true)){j->valid=false;return SJ_STALE;}
    if(n>SFS64_BANK_BYTES)return SJ_FULL;
    if(!sfs64_snapshot_valid(p,n,j->capacity,j->data_start))return SJ_INVALID;
    uint64_t gen=j->generation[j->active];
    if(!gen||gen==UINT64_MAX)return SJ_FULL;
    /* Disjoint trusted input contract; do not permit aliases into the
     * journal object, where preparing the inactive bank would change it. */
    if(n&&((uintptr_t)p>UINTPTR_MAX-n||
        !((uintptr_t)p+n<=(uintptr_t)j||(uintptr_t)j+sizeof *j<=(uintptr_t)p)))return SJ_INVALID;
    unsigned target=j->active^1u;uint8_t *data=j->payload[target];
    zero(data,SFS64_BANK_BYTES);if(n)copy(data,p,n);
    j->valid=false;
    for(unsigned i=0;i<SFS64_BANK_SECTORS;++i)
        if(put(j->block,j->owner,j->token,header_lba(target)+1+i,data+512*i)!=SJ_OK)return SJ_UNCERTAIN;
    uint8_t h[512];header(h,target,gen+1,n,crc(data,n),j->capacity,j->data_start);
    if(put(j->block,j->owner,j->token,header_lba(target),h)!=SJ_OK)return SJ_UNCERTAIN;
    j->generation[target]=gen+1;j->length[target]=n;j->active=target;j->valid=true;
    return SJ_OK;
}
#ifdef STYLEFS_HOST_FORMAT
enum sj_status sfs64_journal_provision(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t cap,uint64_t start)
{
    if(!geometry(cap,start)||!grant(b,owner,token,cap,true))return SJ_STALE;
    uint8_t p[512];
    for(unsigned i=0;i<SFS64_JOURNAL_SECTORS;++i){
        if(bs_read_sector(b,owner,token,i,p)!=BS_OK)return SJ_IO;
        if(!zeros(p,512))return SJ_SIGNATURE;
    }
    header(p,0,1,0,0,cap,start);
    if(put(b,owner,token,header_lba(0),p)!=SJ_OK)return SJ_UNCERTAIN;
    super(p,cap,start);return put(b,owner,token,0,p);
}
#endif
