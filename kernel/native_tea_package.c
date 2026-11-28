#include <styleos/native_tea_package.h>
#include <styleos/native_tea_elf.h>
#include <styleos/native_tea_registry_104.h>

static uint16_t l16(const uint8_t *x){return (uint16_t)x[0]|((uint16_t)x[1]<<8);}
static uint32_t l32(const uint8_t *x){return (uint32_t)l16(x)|((uint32_t)l16(x+2)<<16);}
static int eq(const uint8_t *a,const uint8_t *b,size_t n)
{ unsigned diff=0;for(size_t i=0;i<n;++i)diff|=a[i]^b[i];return !diff; }
static int same_name(const uint8_t *a,size_t n,const char *s)
{ size_t i=0;while(s[i])++i;return n==i && eq(a,(const uint8_t*)s,n); }
static uint32_t crc(const uint8_t *p,size_t n)
{
    uint32_t v=~UINT32_C(0);
    for(size_t i=0;i<n;++i){v^=p[i];for(unsigned k=0;k<8;++k)
        v=(v>>1)^(UINT32_C(0xedb88320)&(0u-(v&1u)));}
    return ~v;
}
struct member {const uint8_t *data;size_t size;size_t start,end;};
static enum ntp_status parse_member(const uint8_t *p,size_t n,size_t *index,
                                    const char *expected,struct member *out)
{
    size_t o=*index;
    if(o>n || n-o<46 || l32(p+o)!=UINT32_C(0x02014b50))return NTP_FORMAT;
    const uint8_t *c=p+o;
    const uint32_t packed=l32(c+20),unpacked=l32(c+24),local=l32(c+42);
    const size_t name=l16(c+28),extra=l16(c+30),comment=l16(c+32);
    if(l16(c+8) || l16(c+10) || (l16(c+6)&8u) || l16(c+34) ||
       packed!=unpacked || !name || extra || comment ||
       (l32(c+38)&0xffffu) || name>n-o-46 ||
       !same_name(c+46,name,expected)) return NTP_FORMAT;
    o+=46+name;
    if(local>n || n-local<30 || l32(p+local)!=UINT32_C(0x04034b50))return NTP_FORMAT;
    const uint8_t *h=p+local;
    const size_t localname=l16(h+26),localextra=l16(h+28);
    if(l16(h+6) || l16(h+8) || localextra || localname!=name ||
       l32(h+14)!=l32(c+16) || l32(h+18)!=packed || l32(h+22)!=unpacked ||
       30+localname>n-local || !eq(h+30,c+46,name))return NTP_FORMAT;
    size_t payload=local+30+localname;
    if(payload>n || packed>n-payload) return NTP_BOUNDS;
    if(crc(p+payload,packed)!=l32(c+16))return NTP_FORMAT;
    *index=o;
    *out=(struct member){p+payload,packed,local,payload+packed};
    return NTP_OK;
}

enum ntp_status ntp_open(const uint8_t *p,size_t n,const uint8_t pkg_sha[32],
                         const uint8_t elf_sha[32],const uint8_t *manifest,
                         size_t manifest_bytes,struct ntp_payload *out)
{
    if(!p || !pkg_sha || !elf_sha || !manifest || !manifest_bytes || !out)
        return NTP_ARGUMENT;
    if(n<22 || n>NTP_MAX_PACKAGE) return NTP_BOUNDS;
    uint8_t digest[32];nte_sha256(p,n,digest);
    if(!eq(digest,pkg_sha,32))return NTP_HASH;
    size_t end=n-22;
    const uint8_t *e=p+end;
    if(l32(e)!=UINT32_C(0x06054b50) || l16(e+4) || l16(e+6) ||
       l16(e+8)!=2 || l16(e+10)!=2 || l16(e+20))return NTP_FORMAT;
    size_t dir=l32(e+16),dirbytes=l32(e+12);
    if(dir>n || dirbytes>n-dir || dir+dirbytes!=end) return NTP_BOUNDS;
    size_t it=dir;
    struct member a,b;
    enum ntp_status rc=parse_member(p,n,&it,"manifest.json",&a);
    if(rc!=NTP_OK)return rc;
    rc=parse_member(p,n,&it,"bin/x86_64/app.elf",&b);
    if(rc!=NTP_OK)return rc;
    if(it!=end || a.start!=0 || a.end!=b.start || b.end!=dir ||
       a.size!=manifest_bytes || !eq(a.data,manifest,manifest_bytes))return NTP_FORMAT;
    nte_sha256(b.data,b.size,digest);
    if(!eq(digest,elf_sha,32))return NTP_HASH;
    *out=(struct ntp_payload){b.data,b.size};
    return NTP_OK;
}

enum ntp_status ntp_disk_header(const uint8_t h[NTP_SECTOR_BYTES],
                                const uint8_t expected[32],uint32_t *n)
{
    if(!h || !expected || !n)return NTP_ARGUMENT;
    static const uint8_t magic[8]={'S','T','E','A','7','6','9',0};
    if(!eq(h,magic,8) || l32(h+8)!=1)return NTP_FORMAT;
    const uint32_t bytes=l32(h+12);
    if(bytes<22 || bytes>NTP_MAX_PACKAGE)return NTP_BOUNDS;
    if(!eq(h+16,expected,32))return NTP_HASH;
    for(size_t j=48;j<NTP_SECTOR_BYTES;++j)if(h[j])return NTP_FORMAT;
    *n=bytes;return NTP_OK;
}

bool ntp_elf_sector(void *context,uint32_t lba,uint8_t out[NTP_SECTOR_BYTES])
{
    const struct ntp_payload *view=context;
    if(!view||!out||!view->elf||view->elf_bytes<120u||view->elf_bytes>8192u)
        return false;
    if(lba>=(view->elf_bytes+NTP_SECTOR_BYTES-1u)/NTP_SECTOR_BYTES)return false;
    size_t offset=(size_t)lba*NTP_SECTOR_BYTES;
    size_t remaining=view->elf_bytes-offset;
    size_t n=remaining<NTP_SECTOR_BYTES?remaining:NTP_SECTOR_BYTES;
    /* Reader destination must not alias the immutable source or descriptor. */
    uintptr_t dst=(uintptr_t)out,src=(uintptr_t)view->elf,v=(uintptr_t)view;
    if(dst>UINTPTR_MAX-NTP_SECTOR_BYTES||src>UINTPTR_MAX-view->elf_bytes||
       v>UINTPTR_MAX-sizeof *view||
       !(dst+NTP_SECTOR_BYTES<=src||src+view->elf_bytes<=dst)||
       !(dst+NTP_SECTOR_BYTES<=v||v+sizeof *view<=dst))return false;
    for(size_t i=0;i<NTP_SECTOR_BYTES;++i)
        out[i]=i<n?view->elf[offset+i]:0;
    return true;
}

static bool span104(const void *p,size_t n){return p&&n&&n<=UINTPTR_MAX-(uintptr_t)p;}
static bool apart104(const void *a,size_t n,const void *b,size_t m){
 return span104(a,n)&&span104(b,m)&&((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static void copy104(void *to,const void *from,size_t n){
 uint8_t *a=to;const uint8_t *b=from;for(size_t i=0;i<n;++i)a[i]=b[i];
}
enum ntr104_status ntr104_register(struct ntr104_registry *r,const struct nts_pin *pin,
 const char *title,bool wait_input,unsigned *slot){
 if(!r||!pin||!title||!slot||!apart104(r,sizeof *r,pin,sizeof *pin)||
    !apart104(r,sizeof *r,slot,sizeof *slot)||!apart104(pin,sizeof *pin,slot,sizeof *slot)||
    pin->package_bytes<22||pin->package_bytes>NTP_MAX_PACKAGE||
    !pin->manifest_bytes||pin->manifest_bytes>NTR104_MANIFEST||
    !apart104(r,sizeof *r,pin->package_sha,32)||!apart104(r,sizeof *r,pin->elf_sha,32)||
    !apart104(r,sizeof *r,pin->manifest,pin->manifest_bytes)||
    !apart104(slot,sizeof *slot,pin->package_sha,32)||!apart104(slot,sizeof *slot,pin->elf_sha,32)||
    !apart104(slot,sizeof *slot,pin->manifest,pin->manifest_bytes))return NTR104_INVALID;
 size_t len=0;while(len<NTR104_NAME&&title[len]){
  unsigned char ch=(unsigned char)title[len];if(ch<0x20||ch==0x7f)return NTR104_INVALID;
  ++len;
 }
 if(!len||len==NTR104_NAME||!apart104(r,sizeof *r,title,len+1)||!apart104(slot,sizeof *slot,title,len+1))
  return NTR104_INVALID;
 unsigned free_slot=NTR104_SLOTS;
 for(unsigned i=0;i<NTR104_SLOTS;++i){
  const struct ntr104_record *e=&r->entries[i];
  if(e->used&&eq(e->package_sha,pin->package_sha,32))return NTR104_DUPLICATE;
  if(!e->used&&free_slot==NTR104_SLOTS)free_slot=i;
 }
 if(free_slot==NTR104_SLOTS)return NTR104_FULL;
 struct ntr104_record *e=&r->entries[free_slot];
 *e=(struct ntr104_record){0};
 e->wait_input=wait_input;e->bytes=pin->package_bytes;e->manifest_bytes=pin->manifest_bytes;
 copy104(e->package_sha,pin->package_sha,32);copy104(e->elf_sha,pin->elf_sha,32);
 copy104(e->manifest,pin->manifest,pin->manifest_bytes);copy104(e->title,title,len);
 e->used=true;*slot=free_slot;return NTR104_OK;
}
bool ntr104_pin(const struct ntr104_registry *r,unsigned i,struct nts_pin *out){
 if(!r||i>=NTR104_SLOTS||!r->entries[i].used||!apart104(r,sizeof *r,out,sizeof *out))return false;
 const struct ntr104_record *e=&r->entries[i];
 *out=(struct nts_pin){e->package_sha,e->elf_sha,e->manifest,e->manifest_bytes,e->bytes};
 return true;
}
enum ntr104_status ntr104_match(const struct ntr104_registry *r,const uint8_t *p,size_t n,unsigned *slot){
 if(!r||n<22||n>NTP_MAX_PACKAGE||!apart104(r,sizeof *r,p,n)||
    !apart104(r,sizeof *r,slot,sizeof *slot)||!apart104(p,n,slot,sizeof *slot))return NTR104_INVALID;
 uint8_t digest[32];nte_sha256(p,n,digest);
 for(unsigned i=0;i<NTR104_SLOTS;++i){
  const struct ntr104_record *e=&r->entries[i];
  if(!e->used||e->bytes!=n||!eq(e->package_sha,digest,32))continue;
  struct ntp_payload view={0};
  if(ntp_open(p,n,e->package_sha,e->elf_sha,e->manifest,e->manifest_bytes,&view)!=NTP_OK)
   return NTR104_PACKAGE;
  *slot=i;return NTR104_OK;
 }
 return NTR104_UNAPPROVED;
}
