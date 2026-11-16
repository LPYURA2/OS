#include "styleos/storage.h"

static const unsigned char super_magic[8]={'S','T','Y','S','T','R','1',0};
static const unsigned char meta_magic[8]={'S','T','Y','M','E','T','A','1'};
static const unsigned char commit_magic[8]={'S','T','Y','C','M','T','1',0};

static void zero(void *p,size_t n)
{ unsigned char *b=p;for(size_t i=0;i<n;++i)b[i]=0; }
static void copy(void *to,const void *from,size_t n)
{ unsigned char *d=to;const unsigned char *s=from;for(size_t i=0;i<n;++i)d[i]=s[i]; }
static int equal(const void *a,const void *b,size_t n)
{ const unsigned char *x=a,*y=b;for(size_t i=0;i<n;++i)if(x[i]!=y[i])return 0;return 1; }
static int zeros(const unsigned char *p,size_t n)
{ for(size_t i=0;i<n;++i)if(p[i])return 0;return 1; }
static uint32_t get32(const unsigned char *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static uint64_t get64(const unsigned char *p)
{ return (uint64_t)get32(p)|((uint64_t)get32(p+4)<<32); }
static void put32(unsigned char *p,uint32_t v)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8)); }
static void put64(unsigned char *p,uint64_t v)
{ put32(p,(uint32_t)v);put32(p+4,(uint32_t)(v>>32)); }
static uint32_t crc_byte(uint32_t crc,unsigned char b)
{
    crc^=b;for(unsigned i=0;i<8;++i)crc=(crc>>1)^((crc&1u)?UINT32_C(0xedb88320):0);
    return crc;
}
uint32_t st_crc32(const void *p,size_t n)
{
    const unsigned char *b=p;uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<n;++i)crc=crc_byte(crc,b[i]);
    return crc^UINT32_MAX;
}
static uint32_t sector_crc(const unsigned char *p,size_t omitted)
{
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<ST_SECTOR_BYTES;++i)
        crc=crc_byte(crc,(i>=omitted && i<omitted+4)?0:p[i]);
    return crc^UINT32_MAX;
}
static int valid_pointer(const void *p,size_t n,size_t alignment)
{
    uintptr_t a=(uintptr_t)p;return p && !(a%alignment) && a<=UINTPTR_MAX-n;
}
static int overlaps(const void *a,size_t an,const void *b,size_t bn)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<y+bn && y<x+an;
}
static enum st_error arguments(const struct st_io *io,union st_workspace *w,
                                struct st_store *s)
{
    if(!valid_pointer(io,sizeof *io,_Alignof(struct st_io)) ||
       !valid_pointer(w,sizeof *w,4096) ||
       !valid_pointer(s,sizeof *s,_Alignof(struct st_store)) ||
       overlaps(w,sizeof *w,s,sizeof *s) || !io->read || !io->write || !io->flush)
        return ST_ARGUMENT;
    return io->sectors==ST_DISK_SECTORS?ST_OK:ST_CAPACITY;
}
static enum st_error live(struct st_store *s,union st_workspace *w)
{
    if(!valid_pointer(s,sizeof *s,_Alignof(struct st_store)))return ST_ARGUMENT;
    enum st_error e=arguments(&s->io,w,s);if(e!=ST_OK)return e;
    if(s->mounted!=1 || s->prepared>1 || s->active < -1 || s->active>=2 ||
       ((s->active==-1)!=(s->generation==0)))return ST_STATE;
    return ST_OK;
}
static void reset(struct st_store *s,const struct st_io *io)
{ zero(s,sizeof *s);s->io=*io;s->active=-1; }
static enum st_error io_uncertain(struct st_store *s)
{ s->mounted=0;s->prepared=0;zero(&s->pending,sizeof s->pending);return ST_IO; }
static uint64_t slot_lba(unsigned slot)
{ return slot==0?ST_SLOT0_LBA:ST_SLOT1_LBA; }
static int valid_name(const unsigned char *p,size_t n)
{
    if(!p || n==0 || n>ST_NAME_MAX)return 0;
    for(size_t i=0;i<n;++i)if(p[i]<0x21u || p[i]>0x7eu || p[i]=='/' || p[i]=='\\')return 0;
    return 1;
}

static void make_super(unsigned char *p)
{
    zero(p,ST_SECTOR_BYTES);copy(p,super_magic,8);put32(p+8,1);put32(p+12,512);
    put64(p+16,ST_DISK_SECTORS);put64(p+24,ST_SLOT0_LBA);put64(p+32,ST_SLOT1_LBA);
    put32(p+40,ST_SLOT_SECTORS);put32(p+44,ST_DATA_MAX);put32(p+48,ST_NAME_MAX);
    put32(p+52,sector_crc(p,52));
}
static enum st_error read_super(const struct st_io *io,union st_workspace *w)
{
    unsigned char *p=w->io.sector;
    if(io->read(io->context,0,p))return ST_IO;
    if(!equal(p,super_magic,8))return ST_SIGNATURE;
    if(get32(p+8)!=1 || get32(p+12)!=512 || get64(p+16)!=ST_DISK_SECTORS ||
       get64(p+24)!=ST_SLOT0_LBA || get64(p+32)!=ST_SLOT1_LBA ||
       get32(p+40)!=ST_SLOT_SECTORS || get32(p+44)!=ST_DATA_MAX ||
       get32(p+48)!=ST_NAME_MAX || get32(p+52)!=sector_crc(p,52) ||
       !zeros(p+56,ST_SECTOR_BYTES-56))return ST_CORRUPT;
    return ST_OK;
}
struct metadata {
    uint64_t generation;
    uint32_t length,name_length,payload_crc,metadata_crc;
    unsigned char name[32];
};
enum slot_status { SLOT_BAD, SLOT_ZERO, SLOT_VALID };
static int read_payload(const struct st_io *io,union st_workspace *w,uint64_t base)
{
    return io->read(io->context,base+1,w->io.payload) ||
           io->read(io->context,base+2,w->io.payload+ST_SECTOR_BYTES);
}
/* ST_OK with an explicit status distinguishes pristine empty from corruption;
 * transport I/O is an error even if the other slot happened to be valid. */
static enum st_error inspect(const struct st_io *io,union st_workspace *w,
    unsigned slot,enum slot_status *status,struct metadata *out)
{
    unsigned char *p=w->io.sector;const uint64_t base=slot_lba(slot);*status=SLOT_BAD;
    if(io->read(io->context,base,p))return ST_IO;
    if(zeros(p,ST_SECTOR_BYTES)){
        if(read_payload(io,w,base) || io->read(io->context,base+3,p))return ST_IO;
        if(zeros(w->io.payload,ST_DATA_MAX) && zeros(p,ST_SECTOR_BYTES))*status=SLOT_ZERO;
        return ST_OK;
    }
    if(!equal(p,meta_magic,8) || get32(p+8)!=1 || get32(p+12)!=512 ||
       get64(p+16)==0 || get32(p+24)>ST_DATA_MAX ||
       get32(p+28)==0 || get32(p+28)>ST_NAME_MAX ||
       get32(p+36)!=sector_crc(p,36) || get32(p+40)!=slot ||
       get32(p+44)!=base || !zeros(p+48,16) || !zeros(p+96,416))return ST_OK;
    struct metadata m={get64(p+16),get32(p+24),get32(p+28),get32(p+32),get32(p+36),{0}};
    copy(m.name,p+64,32);
    if(!valid_name(m.name,m.name_length) || !zeros(m.name+m.name_length,32-m.name_length))return ST_OK;
    if(read_payload(io,w,base))return ST_IO;
    if(st_crc32(w->io.payload,ST_DATA_MAX)!=m.payload_crc ||
       !zeros(w->io.payload+m.length,ST_DATA_MAX-m.length))return ST_OK;
    if(io->read(io->context,base+3,p))return ST_IO;
    if(!equal(p,commit_magic,8) || get32(p+8)!=1 || get32(p+12)!=512 ||
       get64(p+16)!=m.generation || get32(p+24)!=m.length ||
       get32(p+28)!=m.metadata_crc || get32(p+32)!=m.payload_crc ||
       get32(p+36)!=slot || get64(p+40)!=base ||
       get32(p+48)!=sector_crc(p,48) || !zeros(p+52,460))return ST_OK;
    *out=m;*status=SLOT_VALID;return ST_OK;
}

enum st_error st_init(const struct st_io *io,union st_workspace *w,struct st_store *s)
{
    enum st_error e=arguments(io,w,s);if(e!=ST_OK)return e;
    const struct st_io transport=*io;reset(s,&transport);
    /* No byte is written until the entire exact-size disk has been observed
     * zero. A signature error never silently invokes this function. */
    for(uint64_t at=0;at<ST_DISK_SECTORS;++at){
        if(transport.read(transport.context,at,w->io.sector))return ST_IO;
        if(!zeros(w->io.sector,ST_SECTOR_BYTES))return ST_NOT_EMPTY;
    }
    make_super(w->io.sector);
    if(transport.write(transport.context,0,w->io.sector) || transport.flush(transport.context))return io_uncertain(s);
    s->mounted=1;return ST_OK;
}
enum st_error st_mount(const struct st_io *io,union st_workspace *w,struct st_store *s)
{
    enum st_error e=arguments(io,w,s);if(e!=ST_OK)return e;
    const struct st_io transport=*io;reset(s,&transport);
    if((e=read_super(&transport,w))!=ST_OK)return e;
    enum slot_status state[2];struct metadata metadata[2];
    for(unsigned slot=0;slot<2;++slot)
        if((e=inspect(&transport,w,slot,&state[slot],&metadata[slot]))!=ST_OK)return e;
    if(state[0]!=SLOT_VALID && state[1]!=SLOT_VALID){
        if(state[0]==SLOT_ZERO && state[1]==SLOT_ZERO){s->mounted=1;return ST_EMPTY;}
        return ST_CORRUPT;
    }
    if(state[0]==SLOT_VALID && state[1]==SLOT_VALID &&
       metadata[0].generation==metadata[1].generation)return ST_CORRUPT;
    unsigned chosen=state[0]==SLOT_VALID?0:1;
    if(state[1]==SLOT_VALID && (state[0]!=SLOT_VALID || metadata[1].generation>metadata[0].generation))chosen=1;
    s->active=(int)chosen;s->generation=metadata[chosen].generation;s->mounted=1;return ST_OK;
}
enum st_error st_read(struct st_store *s,union st_workspace *w,struct st_record *record)
{
    enum st_error e=live(s,w);if(e!=ST_OK)return e;
    if(!valid_pointer(record,sizeof *record,_Alignof(struct st_record)) ||
       overlaps(record,sizeof *record,w,sizeof *w) || overlaps(record,sizeof *record,s,sizeof *s))return ST_ARGUMENT;
    if(s->active==-1)return ST_EMPTY;
    if((e=read_super(&s->io,w))!=ST_OK)return e;
    enum slot_status status;struct metadata m;
    if((e=inspect(&s->io,w,(unsigned)s->active,&status,&m))!=ST_OK)return e;
    if(status!=SLOT_VALID || m.generation!=s->generation)return ST_CORRUPT;
    zero(record,sizeof *record);record->generation=m.generation;
    record->name_length=m.name_length;record->length=m.length;
    copy(record->name,m.name,32);copy(record->data,w->io.payload,ST_DATA_MAX);return ST_OK;
}
enum st_error st_prepare(struct st_store *s,union st_workspace *w,
    const unsigned char *name,size_t name_length,const unsigned char *data,
    size_t length,struct st_pending *pending)
{
    enum st_error e=live(s,w);if(e!=ST_OK)return e;
    if(s->prepared)return ST_STATE;
    if(!valid_pointer(pending,sizeof *pending,_Alignof(struct st_pending)) ||
       overlaps(pending,sizeof *pending,w,sizeof *w) || overlaps(pending,sizeof *pending,s,sizeof *s) ||
       !valid_pointer(name,name_length,1) || !valid_name(name,name_length) ||
       (length && !valid_pointer(data,length,1)))return ST_ARGUMENT;
    if(length>ST_DATA_MAX)return ST_RANGE;
    if(overlaps(name,name_length,w,sizeof *w) || overlaps(name,name_length,s,sizeof *s) ||
       (length && (overlaps(data,length,w,sizeof *w) || overlaps(data,length,s,sizeof *s))))return ST_ARGUMENT;
    if(s->generation==UINT64_MAX)return ST_OVERFLOW;
    unsigned slot=s->active==0?1u:0u;uint64_t base=slot_lba(slot),gen=s->generation+1;
    /* A corrupt newer candidate can make mount fall back to an older
     * generation. Its old commit might then match this replacement exactly.
     * Make that commit durably invalid BEFORE any replacement bytes reach
     * the disk, including if a later write/flush fails or the guest stops. */
    unsigned char *p=w->io.sector;zero(p,ST_SECTOR_BYTES);
    if(s->io.write(s->io.context,base+3,p) ||
       s->io.flush(s->io.context))return io_uncertain(s);
    zero(w->io.payload,ST_DATA_MAX);if(length)copy(w->io.payload,data,length);
    uint32_t payload_crc=st_crc32(w->io.payload,ST_DATA_MAX);
    zero(p,ST_SECTOR_BYTES);copy(p,meta_magic,8);
    put32(p+8,1);put32(p+12,512);put64(p+16,gen);put32(p+24,(uint32_t)length);
    put32(p+28,(uint32_t)name_length);put32(p+32,payload_crc);
    put32(p+40,slot);put32(p+44,(uint32_t)base);copy(p+64,name,name_length);
    uint32_t metadata_crc=sector_crc(p,36);put32(p+36,metadata_crc);
    if(s->io.write(s->io.context,base,p) ||
       s->io.write(s->io.context,base+1,w->io.payload) ||
       s->io.write(s->io.context,base+2,w->io.payload+ST_SECTOR_BYTES) ||
       s->io.flush(s->io.context))return io_uncertain(s);
    s->pending=(struct st_pending){gen,slot,(uint32_t)length,metadata_crc,payload_crc};
    s->prepared=1;*pending=s->pending;return ST_OK;
}
static int token_equal(const struct st_pending *a,const struct st_pending *b)
{
    return a->generation==b->generation && a->slot==b->slot && a->length==b->length &&
           a->metadata_crc==b->metadata_crc && a->payload_crc==b->payload_crc;
}
enum st_error st_commit(struct st_store *s,union st_workspace *w,const struct st_pending *pending)
{
    enum st_error e=live(s,w);if(e!=ST_OK)return e;
    if(!valid_pointer(pending,sizeof *pending,_Alignof(struct st_pending)))return ST_ARGUMENT;
    if(s->prepared!=1 || !token_equal(pending,&s->pending) ||
       pending->slot>1 || pending->generation!=s->generation+1 || pending->length>ST_DATA_MAX)return ST_STATE;
    const struct st_pending token=*pending;uint64_t base=slot_lba(token.slot);
    unsigned char *p=w->io.sector;zero(p,ST_SECTOR_BYTES);copy(p,commit_magic,8);
    put32(p+8,1);put32(p+12,512);put64(p+16,token.generation);
    put32(p+24,token.length);put32(p+28,token.metadata_crc);put32(p+32,token.payload_crc);
    put32(p+36,token.slot);put64(p+40,base);put32(p+48,sector_crc(p,48));
    if(s->io.write(s->io.context,base+3,p) || s->io.flush(s->io.context))return io_uncertain(s);
    s->active=(int)token.slot;s->generation=token.generation;s->prepared=0;
    zero(&s->pending,sizeof s->pending);return ST_OK;
}
const char *st_error_name(enum st_error error)
{
    static const char *const names[]={"ok","argument","capacity","io","signature",
        "corrupt","not-empty","empty","state","range","overflow"};
    return (unsigned)error<sizeof names/sizeof names[0]?names[error]:"unknown";
}
