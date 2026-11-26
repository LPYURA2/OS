#include <styleos/stylefs_stream64.h>

static uint32_t get32(const uint8_t *p){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t get64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;++i)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
static void copy(uint8_t *p,const uint8_t *q,size_t n){for(size_t i=0;i<n;++i)p[i]=q[i];}
static uint32_t crc(uint32_t c,const uint8_t *p,size_t n){
    for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
    return c;
}
static bool disjoint(const void *p,size_t n,const void *q,size_t m){
    if(!n||!m)return true;
    return p&&q&&n<=UINTPTR_MAX-(uintptr_t)p&&m<=UINTPTR_MAX-(uintptr_t)q&&
        ((uintptr_t)p+n<=(uintptr_t)q||(uintptr_t)q+m<=(uintptr_t)p);
}
static bool next(const struct sfs64_journal *j,unsigned bank,size_t *at,struct sfs64_map *m)
{
    size_t n=j->length[bank];
    if(n>SFS64_BANK_BYTES||*at>n||n-*at<4)return false;
    const uint8_t *p=j->payload[bank]+*at;
    uint32_t bytes=get32(p);*at+=4;
    if(bytes>n-*at||!sfs64_map_decode(p+4,bytes,j->capacity,j->data_start,m))return false;
    *at+=bytes;return true;
}
static bool find(const struct sfs64_journal *j,uint64_t id,struct sfs64_map *m)
{
    for(size_t at=0;at<j->length[j->active];){
        if(!next(j,j->active,&at,m))return false;
        if(m->file.id==id)return true;
    }
    return false;
}
static bool inventory(struct sfs64_stream *t,struct sfs64_journal *j,unsigned bank,size_t *count)
{
    *count=0;
    if(!j->generation[bank])return true;
    if(!sfs64_snapshot_valid(j->payload[bank],j->length[bank],j->capacity,j->data_start))return false;
    for(size_t at=0;at<j->length[bank];){
        size_t before=at;struct sfs64_map m;
        if(!next(j,bank,&at,&m))return false;
        for(size_t i=0;i<m.count;++i){
            if(*count==SFS64_STREAM_RUNS)return false;
            const uint8_t *p=m.records+i*SFS64_MAP_RECORD;
            struct sfs64_run r={get64(p),get64(p+8)};
            size_t k=*count;
            while(k&&t->occupied[bank][k-1].first>r.first){
                t->occupied[bank][k]=t->occupied[bank][k-1];--k;
            }
            t->occupied[bank][k]=r;++*count;
        }
        if(bank==j->active&&m.file.id!=t->file.id){
            size_t n=at-before;
            if(n>SFS64_BANK_BYTES-t->prefix)return false;
            copy(t->candidate+t->prefix,j->payload[bank]+before,n);t->prefix+=n;
        }
    }
    return true;
}
static bool live(struct sfs64_stream *t)
{
    return t&&t->active&&sfs64_journal_ready(t->journal,true)&&
           t->journal->writer==t&&t->journal->token==t->token&&
           t->journal->generation[t->journal->active]==t->generation;
}
void sfs64_stream_cancel(struct sfs64_stream *t)
{
    if(!t||!t->active)return;
    if(t->journal&&t->journal->writer==t)t->journal->writer=0;
    t->active=false;
}
static enum sj_status fail(struct sfs64_stream *t)
{
    if(t->journal&&t->journal->writer==t)t->journal->valid=false;
    sfs64_stream_cancel(t);return SJ_UNCERTAIN;
}
enum sj_status sfs64_stream_begin(struct sfs64_stream *t,struct sfs64_journal *j,
                                  uint64_t id,uint64_t expected_revision,uint64_t bytes)
{
    if(!t||!j||!id||t->active||!disjoint(t,sizeof *t,j,sizeof *j))return SJ_INVALID;
    if(!sfs64_journal_ready(j,true)||j->writer)return SJ_STALE;
    if(!disjoint(t,sizeof *t,j->block,sizeof *j->block))return SJ_INVALID;
    uint64_t generation=j->generation[j->active];
    if(!generation||generation==UINT64_MAX||expected_revision==UINT64_MAX)return SJ_FULL;
    if(!sfs64_snapshot_valid(j->payload[j->active],j->length[j->active],j->capacity,j->data_start))
        return SJ_CORRUPT;
    struct sfs64_map old;
    bool exists=find(j,id,&old);
    if((exists&&old.file.revision!=expected_revision)||(!exists&&expected_revision))return SJ_STALE;
    if(exists&&!expected_revision)return SJ_STALE;
    t->file=(struct sfs64_file){id,expected_revision+1,bytes,0};
    t->prefix=0;t->count=0;
    size_t counts[2];
    for(unsigned i=0;i<2;++i)if(!inventory(t,j,i,&counts[i]))return SJ_CORRUPT;
    struct sfs64_inventory a={t->occupied[0],counts[0]},b={t->occupied[1],counts[1]};
    if(SFS64_BANK_BYTES-t->prefix<4+SFS64_MAP_HEADER)return SJ_FULL;
    size_t slots=(SFS64_BANK_BYTES-t->prefix-4-SFS64_MAP_HEADER)/SFS64_MAP_RECORD;
    enum sfs64_alloc_status r=sfs64_allocate(j->capacity,j->data_start,a,b,bytes,t->runs,slots,&t->count);
    if(r!=SFS64_ALLOC_OK)return r==SFS64_ALLOC_INVALID?SJ_CORRUPT:SJ_FULL;
    t->manifest=sfs64_map_length(t->count);
    if(!t->manifest||t->manifest>SFS64_BANK_BYTES-t->prefix-4)return SJ_FULL;
    put32(t->candidate+t->prefix,(uint32_t)t->manifest);
    if(!sfs64_map_encode(t->candidate+t->prefix+4,t->manifest,t->file,j->capacity,j->data_start,t->runs,t->count))
        return SJ_CORRUPT;
    if(!sfs64_snapshot_valid(t->candidate,t->prefix+4+t->manifest,j->capacity,j->data_start))return SJ_CORRUPT;
    t->journal=j;t->token=j->token;t->generation=generation;t->received=0;
    t->running_crc=UINT32_MAX;t->active=true;j->writer=t;return SJ_OK;
}
static enum sj_status append(struct sfs64_stream *t,const uint8_t *p,size_t n,bool batch)
{
    if(!live(t)){sfs64_stream_cancel(t);return SJ_STALE;}
    struct sfs64_journal *j=t->journal;
    if(!n||n>SFS64_WINDOW_BYTES||t->received>t->file.bytes||n>t->file.bytes-t->received||
       t->received%512||(n%512&&n!=t->file.bytes-t->received)||
       !disjoint(p,n,t,sizeof *t)||!disjoint(p,n,j,sizeof *j)||
       !disjoint(p,n,j->block,sizeof *j->block))return SJ_INVALID;
    struct sfs64_map m;
    if(!sfs64_map_decode(t->candidate+t->prefix+4,t->manifest,j->capacity,j->data_start,&m))return fail(t);
    size_t done=0;
    while(done<n){
        struct sfs_extent64 e;uint64_t local;size_t count,written=0;
        if(!sfs64_map_chunk(&m,t->received+done,n-done,&e,&local,&count))return fail(t);
        enum bs_status r=batch?
            sfs64_stage_batch(j->block,j->owner,j->token,e,local,p+done,count,&written):
            sfs64_stage_window(j->block,j->owner,j->token,e,local,p+done,count,&written);
        if(r!=BS_OK||written!=count)return fail(t);
        /* Verify the complete durable sectors, including canonical EOF tail. */
        for(size_t at=0;at<count;at+=512){
            uint8_t check[512];
            if(bs_read_sector(j->block,j->owner,j->token,e.first+local/512+at/512,check)!=BS_OK)
                return fail(t);
            size_t used=count-at<512?count-at:512;
            for(size_t k=0;k<512;++k)
                if(check[k]!=(k<used?p[done+at+k]:0))return fail(t);
        }
        done+=count;
    }
    t->running_crc=crc(t->running_crc,p,n);t->received+=n;return SJ_OK;
}
enum sj_status sfs64_stream_append(struct sfs64_stream *t,const uint8_t *p,size_t n)
{return append(t,p,n,false);}
enum sj_status sfs64_stream_append_batched(struct sfs64_stream *t,const uint8_t *p,size_t n)
{return append(t,p,n,true);}
enum sj_status sfs64_stream_finish(struct sfs64_stream *t)
{
    if(!live(t)){sfs64_stream_cancel(t);return SJ_STALE;}
    if(t->received!=t->file.bytes)return SJ_INVALID;
    struct sfs64_journal *j=t->journal;
    t->file.checksum=~t->running_crc;
    if(!sfs64_map_encode(t->candidate+t->prefix+4,t->manifest,t->file,j->capacity,j->data_start,t->runs,t->count))
        return fail(t);
    /* Synchronous trusted call, no competing writer can enter between
     * relinquishing the stream guard and journal publication. */
    j->writer=0;t->active=false;
    return sfs64_journal_publish(j,t->candidate,t->prefix+4+t->manifest);
}
enum sj_status sfs64_stream_stat(struct sfs64_journal *j,uint64_t id,struct sfs64_file *out)
{
    if(!id||!out||!j||!disjoint(out,sizeof *out,j,sizeof *j))return SJ_INVALID;
    if(!sfs64_journal_ready(j,false))return SJ_STALE;
    struct sfs64_map m;
    if(!find(j,id,&m))return SJ_STALE;
    *out=m.file;return SJ_OK;
}
enum bs_status sfs64_stream_read(struct sfs64_journal *j,uint64_t id,uint64_t revision,
                                uint64_t offset,uint8_t *out,size_t wanted,size_t *completed)
{
    if(!j||!id||!revision||!completed||!wanted||
       !disjoint(completed,sizeof *completed,j,sizeof *j)||
       !disjoint(out,wanted,j,sizeof *j)||
       !disjoint(out,wanted,completed,sizeof *completed))return BS_INVALID;
    if(!sfs64_journal_ready(j,false))return BS_STALE;
    if(!disjoint(completed,sizeof *completed,j->block,sizeof *j->block)||
       !disjoint(out,wanted,j->block,sizeof *j->block))return BS_INVALID;
    *completed=0;struct sfs64_map m;
    if(!find(j,id,&m)||m.file.revision!=revision)return BS_STALE;
    struct sfs_extent64 e;uint64_t local;size_t n;
    if(!sfs64_map_chunk(&m,offset,wanted,&e,&local,&n))return BS_RANGE;
    return sfs64_read_window(j->block,j->owner,j->token,e,local,out,n,completed);
}
