#include <styleos/native_tea_elf.h>

/* Minimal SHA-256 for bounded executable-byte verification. This code does
 * not authenticate a package author and does not parse a .tea ZIP itself. */
static uint32_t ror(uint32_t x,unsigned n){return (x>>n)|(x<<(32u-n));}
static uint32_t be32(const uint8_t *p)
{ return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
static uint16_t le16(const uint8_t *p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|
                                         (uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t le64(const uint8_t *p){return (uint64_t)le32(p)|((uint64_t)le32(p+4)<<32);}

static const uint32_t k[64]={
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
static void sha_block(uint32_t h[8],const uint8_t p[64])
{
    uint32_t w[64];
    for(unsigned i=0;i<16;i++)w[i]=be32(p+4u*i);
    for(unsigned i=16;i<64;i++){
        uint32_t s0=ror(w[i-15],7)^ror(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=ror(w[i-2],17)^ror(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for(unsigned i=0;i<64;i++){
        uint32_t s1=ror(e,6)^ror(e,11)^ror(e,25);
        uint32_t ch=(e&f)^(~e&g);
        uint32_t t1=hh+s1+ch+k[i]+w[i];
        uint32_t s0=ror(a,2)^ror(a,13)^ror(a,22);
        uint32_t maj=(a&b)^(a&c)^(b&c);
        uint32_t t2=s0+maj;
        hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
}
void nte_sha256(const uint8_t *input,size_t bytes,uint8_t out[32])
{
    uint32_t h[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                   0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    size_t i=0;
    while(bytes-i>=64u){sha_block(h,input+i);i+=64u;}
    uint8_t tail[128]={0};
    size_t left=bytes-i;
    for(size_t j=0;j<left;++j)tail[j]=input[i+j];
    tail[left]=0x80u;
    const size_t last=left>=56u?120u:56u;
    const uint64_t bits=(uint64_t)bytes*8u;
    for(unsigned j=0;j<8;++j)tail[last+j]=(uint8_t)(bits>>(56u-8u*j));
    sha_block(h,tail);
    if(last==120u)sha_block(h,tail+64);
    for(unsigned j=0;j<8;++j){
        out[4u*j]=(uint8_t)(h[j]>>24);
        out[4u*j+1]=(uint8_t)(h[j]>>16);
        out[4u*j+2]=(uint8_t)(h[j]>>8);
        out[4u*j+3]=(uint8_t)h[j];
    }
}

static bool nte_span(const void *ptr,size_t n){
 return ptr&&n&&n<=UINTPTR_MAX-(uintptr_t)ptr;
}
static bool nte_distinct(const void *a,size_t n,const void *b,size_t m){
 return nte_span(a,n)&&nte_span(b,m)&&
  ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static enum nte_status load_page(const uint8_t *elf,size_t elf_bytes,
                                  const uint8_t hash[32],uint8_t *page,
                                  size_t page_bytes,struct nte_loaded *loaded,bool allow_entry_offset)
{
    if(!elf || !hash || !page || !loaded || page_bytes!=NTE_PAGE ||
       elf_bytes<120u || elf_bytes>16u*1024u*1024u ||
       (uintptr_t)page%NTE_PAGE) return NTE_ARGUMENT;
    if(allow_entry_offset&&(
       (uintptr_t)loaded%_Alignof(struct nte_loaded)||
       !nte_distinct(page,page_bytes,elf,elf_bytes)||
       !nte_distinct(page,page_bytes,hash,32)||
       !nte_distinct(loaded,sizeof *loaded,page,page_bytes)||
       !nte_distinct(loaded,sizeof *loaded,elf,elf_bytes)||
       !nte_distinct(loaded,sizeof *loaded,hash,32)))return NTE_ARGUMENT;
    if(elf[0]!=127u || elf[1]!='E' || elf[2]!='L' || elf[3]!='F' ||
       elf[4]!=2u || elf[5]!=1u || elf[6]!=1u ||
       le16(elf+16)!=2u || le16(elf+18)!=62u || le32(elf+20)!=1u ||
       le16(elf+52)!=64u || le16(elf+54)!=56u || le16(elf+56)!=1u)
        return NTE_FORMAT;
    const uint64_t phoff=le64(elf+32),entry=le64(elf+24);
    if(phoff<64u || phoff>elf_bytes || elf_bytes-phoff<56u)return NTE_BOUNDS;
    const uint8_t *ph=elf+(size_t)phoff;
    const uint32_t type=le32(ph),flags=le32(ph+4);
    const uint64_t offset=le64(ph+8),vaddr=le64(ph+16);
    const uint64_t filesz=le64(ph+32),memsz=le64(ph+40),align=le64(ph+48);
    if(type!=1u || flags!=5u || vaddr!=NTE_USER_CODE_VA ||
       (!allow_entry_offset&&entry!=NTE_USER_CODE_VA) || align!=NTE_PAGE ||
       filesz==0 || filesz>memsz || memsz>NTE_PAGE)
        return NTE_RIGHTS;
    if(entry<vaddr||entry-vaddr>=filesz)return NTE_RIGHTS;
    if(offset>elf_bytes || filesz>elf_bytes-offset ||
       (offset&(NTE_PAGE-1u)) || memsz>UINT32_MAX) return NTE_BOUNDS;
    uint8_t actual[32];nte_sha256(elf,elf_bytes,actual);
    unsigned difference=0;
    for(unsigned j=0;j<32;j++)difference|=actual[j]^hash[j];
    if(difference)return NTE_HASH;
    /* Validation is complete. No write to page/loaded on any rejection. */
    for(size_t i=0;i<NTE_PAGE;i++)page[i]=0;
    for(size_t i=0;i<(size_t)filesz;i++)page[i]=elf[offset+i];
    *loaded=(struct nte_loaded){entry,(uint32_t)filesz,(uint32_t)memsz};
    return NTE_OK;
}

enum nte_status nte_load_x64_page(const uint8_t *elf,size_t bytes,const uint8_t hash[32],uint8_t *page,size_t page_bytes,struct nte_loaded *loaded){
 return load_page(elf,bytes,hash,page,page_bytes,loaded,false);
}
enum nte_status nte_load_x64_image(const uint8_t *elf,size_t bytes,const uint8_t hash[32],uint8_t *page,size_t page_bytes,struct nte_loaded *loaded){
 return load_page(elf,bytes,hash,page,page_bytes,loaded,true);
}
