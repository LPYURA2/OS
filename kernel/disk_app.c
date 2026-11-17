#include "styleos/disk_app.h"

static uint32_t get32(const unsigned char *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static unsigned get16(const unsigned char *p)
{ return (unsigned)p[0]|((unsigned)p[1]<<8); }
static int valid(const void *p,size_t n,size_t alignment)
{ uintptr_t a=(uintptr_t)p;return p && !(a%alignment) && n<=UINTPTR_MAX-a; }
static int overlap(const void *a,size_t an,const void *b,size_t bn)
{ uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<y+bn && y<x+an; }
static uint32_t checksum(const unsigned char *p,size_t n)
{
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<n;++i){
        crc^=(i>=28 && i<32)?0:p[i];
        for(unsigned bit=0;bit<8;++bit)
            crc=(crc>>1)^((crc&1u)?UINT32_C(0xedb88320):0);
    }
    return crc^UINT32_MAX;
}
static enum da_result inspect(const unsigned char *p,size_t n,struct da_info *out)
{
    static const unsigned char magic[8]={'S','T','Y','A','P','P','1',0};
    if(n<DA_HEADER_BYTES || n>DA_FILE_MAX)return DA_SIZE;
    for(unsigned i=0;i<8;++i)if(p[i]!=magic[i])return DA_MAGIC;
    if(get32(p+8)!=1)return DA_VERSION;
    if(get16(p+12)!=DA_ISA_AARCH64)return DA_ISA;
    if(get16(p+14)!=DA_HEADER_BYTES || get32(p+24)!=0)return DA_HEADER;
    uint32_t length=get32(p+16),entry=get32(p+20);
    /* Subtraction follows the minimum-header check: no input-controlled sum. */
    if(length==0 || length>DA_CODE_MAX || (length&3u) || length!=n-DA_HEADER_BYTES)
        return DA_SIZE;
    if((entry&3u) || entry>=length || entry!=0)return DA_ENTRY;
    uint32_t crc=checksum(p,n);if(get32(p+28)!=crc)return DA_CHECKSUM;
    if(length!=DA_CODE_BYTES)return DA_OPCODE;
    const unsigned char *code=p+DA_HEADER_BYTES;
    uint32_t lo=get32(code+12),hi=get32(code+16),exit=get32(code+20);
    /* This deliberately finite grammar proves every executable instruction.
     * CBNZ X0,+12; MOVZ X1,#0x1201,LSL16; LDR X2,[X1]; marker low/high;
     * MOVZ X0,#exit; SVC #2. Branch and load register/offsets are immutable. */
    if(get32(code)!=UINT32_C(0xb5000060) ||
       get32(code+4)!=UINT32_C(0xd2a24021) ||
       get32(code+8)!=UINT32_C(0xf9400022) ||
       (lo&UINT32_C(0xffe0001f))!=UINT32_C(0xd2800013) ||
       (hi&UINT32_C(0xffe0001f))!=UINT32_C(0xf2a00013) ||
       (exit&UINT32_C(0xffe0001f))!=UINT32_C(0xd2800000) ||
       get32(code+24)!=UINT32_C(0xd4000041))return DA_OPCODE;
    *out=(struct da_info){length,entry,8,24,
        ((lo>>5)&0xffffu)|(((hi>>5)&0xffffu)<<16),(exit>>5)&0xffffu,crc};
    return DA_OK;
}
enum da_result da_validate(const void *source,size_t n,struct da_info *info)
{
    if(!valid(source,n,1) || !valid(info,sizeof *info,_Alignof(struct da_info)))return DA_ARGUMENT;
    if(overlap(source,n,info,sizeof *info))return DA_OVERLAP;
    struct da_info result;enum da_result e=inspect(source,n,&result);
    if(e==DA_OK)*info=result;
    return e;
}
enum da_result da_copy(const void *source,size_t n,void *destination,size_t capacity,
                        struct da_info *info)
{
    if(!valid(source,n,1) || !valid(destination,capacity,4) ||
       !valid(info,sizeof *info,_Alignof(struct da_info)))return DA_ARGUMENT;
    if(overlap(source,n,info,sizeof *info) || overlap(source,n,destination,capacity) ||
       overlap(destination,capacity,info,sizeof *info))return DA_OVERLAP;
    struct da_info result;enum da_result e=inspect(source,n,&result);
    if(e!=DA_OK)return e;
    if(capacity<result.code_length)return DA_CAPACITY;
    unsigned char *d=destination;const unsigned char *s=source;
    for(size_t i=0;i<result.code_length;++i)d[i]=s[DA_HEADER_BYTES+i];
    *info=result;return DA_OK;
}
const char *da_result_name(enum da_result e)
{
    static const char *const names[]={"OK","ARGUMENT","SIZE","MAGIC","VERSION","ISA",
        "HEADER","ENTRY","CHECKSUM","OPCODE","CAPACITY","OVERLAP"};
    return (unsigned)e<sizeof names/sizeof names[0]?names[e]:"UNKNOWN";
}
