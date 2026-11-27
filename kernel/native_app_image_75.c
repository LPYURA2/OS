#include <styleos/native_app_image_75.h>
static uint16_t u16(const uint8_t *p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|
 (uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t u64(const uint8_t *p){return (uint64_t)u32(p)|((uint64_t)u32(p+4)<<32);}
static bool span(const void *p,size_t n){return p&&n&&n<=UINTPTR_MAX-(uintptr_t)p;}
static bool apart(const void *a,size_t n,const void *b,size_t m){
 return span(a,n)&&span(b,m)&&((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
struct segment {uint32_t offset,file,mem,pages;};
static enum nte_status segment(const uint8_t *ph,size_t bytes,uint64_t floor,
 uint64_t va,unsigned flags,unsigned max,struct segment *out){
 const uint64_t off=u64(ph+8),file=u64(ph+32),mem=u64(ph+40);
 if(u32(ph)!=1||u32(ph+4)!=flags||u64(ph+16)!=va||u64(ph+48)!=NTE_PAGE||
    !mem||mem>(uint64_t)max*NTE_PAGE||file>mem||(flags==5&&!file))return NTE_RIGHTS;
 if(off<floor||off>bytes||file>bytes-off||(off&(NTE_PAGE-1u)))return NTE_BOUNDS;
 *out=(struct segment){(uint32_t)off,(uint32_t)file,(uint32_t)mem,
                        (uint32_t)((mem+NTE_PAGE-1u)/NTE_PAGE)};
 return NTE_OK;
}
enum nte_status nai75_inspect(const uint8_t *elf,size_t bytes,const uint8_t hash[32],
 unsigned stack,struct nai75_layout *out){
 if(bytes<176||bytes>NAI75_MAX_ELF||!stack||stack>NAI75_STACK_PAGES||
    !span(elf,bytes)||!span(hash,32)||!out||
    (uintptr_t)out%_Alignof(struct nai75_layout)||
    !apart(out,sizeof *out,elf,bytes)||!apart(out,sizeof *out,hash,32))return NTE_ARGUMENT;
 if(elf[0]!=127||elf[1]!='E'||elf[2]!='L'||elf[3]!='F'||elf[4]!=2||elf[5]!=1||
    elf[6]!=1||u16(elf+16)!=2||u16(elf+18)!=62||u32(elf+20)!=1||
    u16(elf+52)!=64||u16(elf+54)!=56||u16(elf+56)!=2)return NTE_FORMAT;
 uint64_t phoff=u64(elf+32),entry=u64(elf+24);
 if(phoff<64||phoff>bytes||bytes-phoff<112)return NTE_BOUNDS;
 struct segment code,data;
 enum nte_status st=segment(elf+(size_t)phoff,bytes,phoff+112,
  NTE_USER_CODE_VA,5,NAI75_CODE_PAGES,&code);
 if(st!=NTE_OK)return st;
 st=segment(elf+(size_t)phoff+56,bytes,phoff+112,
  NAI75_DATA_VA,6,NAI75_DATA_PAGES,&data);
 if(st!=NTE_OK)return st;
 if(entry<NTE_USER_CODE_VA||entry-NTE_USER_CODE_VA>=code.file)return NTE_RIGHTS;
 /* Canonical ordered file extents; no header alias, shared payload, or
  * overlapping segment bytes, including the empty BSS-only data case. */
 if(data.offset<(uint64_t)code.offset+code.file)return NTE_BOUNDS;
 uint8_t actual[32];nte_sha256(elf,bytes,actual);
 unsigned diff=0;for(unsigned i=0;i<32;++i)diff|=actual[i]^hash[i];
 if(diff)return NTE_HASH;
 *out=(struct nai75_layout){entry,code.pages,data.pages,stack,
  code.pages+data.pages+stack,code.offset,code.file,code.mem,
  data.offset,data.file,data.mem};
 return NTE_OK;
}
enum nte_status nai75_stage(const struct s844_arena *arena,struct s844_lease lease,
 const uint8_t *elf,size_t bytes,const uint8_t hash[32],unsigned stack,struct nai75_layout *out){
 if(!arena||!out||(uintptr_t)out%_Alignof(struct nai75_layout)||
    !s844_lease_live(arena,lease))return NTE_ARGUMENT;
 const void *memory=(const void *)(uintptr_t)arena->base;
 const void *ptr[]={arena,memory,elf,hash,out};
 const size_t size[]={sizeof *arena,S844_BYTES,bytes,32,sizeof *out};
 for(unsigned i=0;i<5;++i)for(unsigned j=0;j<i;++j)
  if(!apart(ptr[i],size[i],ptr[j],size[j]))return NTE_ARGUMENT;
 struct nai75_layout plan;
 enum nte_status st=nai75_inspect(elf,bytes,hash,stack,&plan);
 if(st!=NTE_OK)return st;
 if(lease.pages!=plan.total_pages)return NTE_BOUNDS;
 /* All refusal points precede any detached lease writes. Exact live lease
  * is independently established; publishing a PID/CR3 is out of scope. */
 volatile uint8_t *dst=(void *)(uintptr_t)lease.pa;
 for(size_t i=0;i<(size_t)lease.pages*NTE_PAGE;++i)dst[i]=0;
 for(size_t i=0;i<plan.code_file;++i)dst[i]=elf[plan.code_offset+i];
 size_t data_start=(size_t)plan.code_pages*NTE_PAGE;
 for(size_t i=0;i<plan.data_file;++i)dst[data_start+i]=elf[plan.data_offset+i];
 *out=plan;return NTE_OK;
}
