#include <styleos/apic_pe_842.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#ifndef MAP_32BIT
#error Host positive proof needs Linux x86_64 MAP_32BIT
#endif

#define N 40u
#define P 4096u
static unsigned char *image;
static unsigned char *stackbuf;
static void u16(unsigned char *b,unsigned off,unsigned val)
{ b[off]=(unsigned char)val; b[off+1]=(unsigned char)(val>>8); }
static void u32(unsigned char *b,unsigned off,unsigned val)
{ for(unsigned i=0;i<4;++i)b[off+i]=(unsigned char)(val>>(i*8u)); }
static void pe(void)
{
    memset(image,0,N*P);
    u16(image,0,0x5a4d);u32(image,0x3c,0x80);u32(image,0x80,0x4550);
    u16(image,0x80+6,3u);u16(image,0x80+20,0xf0);
    u16(image,0x80+24,0x20b);
    u32(image,0x80+24+56,N*P);
    u32(image,0x80+24+60,0x400u);
    const unsigned s=0x80+24+0xf0;
    u32(image,s+8,2u*P);u32(image,s+12,P);u32(image,s+36,0x60000000u);
    u32(image,s+40+8,P);u32(image,s+40+12,3u*P);u32(image,s+40+36,0x40000000u);
    u32(image,s+80+8,36u*P);u32(image,s+80+12,4u*P);u32(image,s+80+36,0xc0000000u);
}
static struct s790_low_request request(void)
{
    return (struct s790_low_request){
        .image={ (uint64_t)(uintptr_t)image, (uint64_t)(uintptr_t)image+N*P },
        .boot_stack={ (uint64_t)(uintptr_t)stackbuf, (uint64_t)(uintptr_t)stackbuf+2u*P},
        .loaded_image=image,.owned_tables=(uint64_t (*)[512])(void*)(image+8u*P),
        .owned_tables_pa=(uint64_t)(uintptr_t)(image+8u*P),
        .required_exec=(uint64_t)(uintptr_t)(image+P+64u),
        .required_write=(uint64_t)(uintptr_t)(image+5u*P+64u),
        .required_rsp=(uint64_t)(uintptr_t)(stackbuf+P+64u)};
}
static void u64(unsigned char *b,unsigned off,uint64_t val){
 for(unsigned i=0;i<8;i++)b[off+i]=(unsigned char)(val>>(8*i));
}
static void descriptor(unsigned char *b,unsigned type,uint64_t pa,uint64_t pages,uint64_t attr){
 memset(b,0,48);u32(b,0,type);u64(b,8,pa);u64(b,24,pages);u64(b,32,attr);
}
/* The loaded PE has four rights runs. An adjacent external RW/NX stack
 * legitimately merges with the last run; a separated stack must not.
 * Check the exact extents/rights in both layouts, not an ASLR-dependent count. */
static bool exact_plan(const struct s842_root *r,bool adjacent){
 const uint64_t base=(uintptr_t)image,stack=(uintptr_t)stackbuf;
 const struct pg_mapping expected[]={
  {base,base,P,0},{base+P,base+P,2u*P,PG_EXEC},
  {base+3u*P,base+3u*P,P,0},
  {base+4u*P,base+4u*P,(36u+(adjacent?2u:0u))*P,PG_WRITE},
  {stack,stack,2u*P,PG_WRITE},
  {0xfee00000,0xfee00000,P,PG_WRITE},{0xfec00000,0xfec00000,P,PG_WRITE}};
 if(stack!=base+(N+(adjacent?0u:1u))*P||r->plan.count!=(adjacent?6u:7u))return false;
 for(size_t i=0,j=0;i<7;i++){
  if(adjacent&&i==4)continue;
  const struct pg_mapping *a=&r->plan.maps[j++],*b=&expected[i];
  if(a->va!=b->va||a->pa!=b->pa||a->bytes!=b->bytes||a->flags!=b->flags)return false;
 }
 return true;
}
int main(void){
 /* One reservation makes adjacency and the later one-page gap deterministic.
  * Keep the gap inaccessible in the host too; only named spans are admitted. */
 image=mmap(NULL,(N+3u)*P,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 assert(image!=MAP_FAILED);
 assert(mprotect(image,N*P,PROT_READ|PROT_WRITE)==0);
 stackbuf=image+N*P;
 assert(mprotect(stackbuf,2u*P,PROT_READ|PROT_WRITE)==0);pe();
 struct s790_low_request p=request();
 struct boot_memory_map m={.ram={p.image,p.boot_stack},.reserved={p.image,p.boot_stack},.nr_ram=2,.nr_reserved=2};
 struct s842_cache c={.pat=0x0007040600070406,.cap=1,.def=0x806,.count=1,.bits=48,.pairs={{0x80000000,0xffff80000800}}};
 struct acpi806_topology t={.enabled_lapics=1,.ioapic_count=1,.lapic_physical=0xfee00000,.ioapic={{0,0xfec00000,0}}};
 struct s842_root r;uint64_t msr=0xfee00900;unsigned n=0;
 #define CHECK(x) do{assert(x);n++;}while(0)
 CHECK(s842_build_pe(&p,&m,&c,&t,msr,&r));CHECK(s842_audit(&r,&c));
 CHECK(exact_plan(&r,true));
 /* Move the stack across a genuine unmapped page within the reservation. */
 assert(mprotect(stackbuf,2u*P,PROT_NONE)==0);stackbuf+=P;
 assert(mprotect(stackbuf,2u*P,PROT_READ|PROT_WRITE)==0);
 p=request();m=(struct boot_memory_map){.ram={p.image,p.boot_stack},.reserved={p.image,p.boot_stack},.nr_ram=2,.nr_reserved=2};
 CHECK(s842_build_pe(&p,&m,&c,&t,msr,&r));CHECK(s842_audit(&r,&c));
 CHECK(exact_plan(&r,false));
 struct pg_lookup gap={0};CHECK(pg_lookup(&r.plan,p.image.end,&gap)!=PG_OK);
 CHECK(r.lapic==t.lapic_physical&&r.ioapic==t.ioapic[0].physical);
 CHECK(!s842_build_pe(&p,&m,&c,&t,msr|0x400,&r)&&!r.ready);
 CHECK(!s842_build_pe(&p,&m,&c,&t,msr^0x1000,&r));
 CHECK(!s842_build_pe(&p,&m,&c,&t,msr&~0x800,&r));
 t.enabled_lapics=2;CHECK(!s842_build_pe(&p,&m,&c,&t,msr,&r));t.enabled_lapics=1;
 t.ioapic_count=2;CHECK(!s842_build_pe(&p,&m,&c,&t,msr,&r));t.ioapic_count=1;
 m.nr_reserved=1;CHECK(!s842_build_pe(&p,&m,&c,&t,msr,&r));m.nr_reserved=2;
 m.nr_candidates=1;m.candidates[0]=p.image;CHECK(!s842_build_pe(&p,&m,&c,&t,msr,&r));m.nr_candidates=0;
 struct s790_low_request q=p;q.boot_stack=(struct boot_range){p.owned_tables_pa,p.owned_tables_pa+P};q.required_rsp=q.boot_stack.start+32;CHECK(!s842_build_pe(&q,&m,&c,&t,msr,&r));
 q=p;q.required_exec=q.required_write;CHECK(!s842_build_pe(&q,&m,&c,&t,msr,&r));
 u32(image,0x80+24+0xf0+36,0xe0000000u);CHECK(!s842_build_pe(&p,&m,&c,&t,msr,&r));pe();
 CHECK(s842_build_pe(&p,&m,&c,&t,msr,&r));
 /* Real normalized UEFI semantics: loader pages are reserved, not
  * conventional RAM. Source is a separate EfiLoaderData allocation. */
 q=p;q.boot_stack=(struct boot_range){p.image.start+6*P,p.image.start+8*P};q.required_rsp=q.boot_stack.start+P;
 struct boot_info *boot=(struct boot_info *)(void *)(image+4*P+128);
 unsigned char *raw=stackbuf;
 *boot=(struct boot_info){.magic=STYLEOS_BOOT_MAGIC,.version=1,.size=sizeof(*boot),
 .arch=STYLEOS_ARCH_X86_64,.platform=STYLEOS_PLATFORM_UEFI,.flags=STYLEOS_BOOT_EXITED_SERVICES,
 .memory_map=(uintptr_t)raw,.memory_map_size=96,.descriptor_size=48,.descriptor_version=1};
 descriptor(raw,1,p.image.start,N,15);descriptor(raw+48,2,(uintptr_t)raw,2,15);
 struct boot_memory_map live={.reserved={p.image,{(uintptr_t)raw,(uintptr_t)raw+2*P}},.nr_reserved=2};
 struct boot_memory_evidence ev={.live={q.image,q.boot_stack,{(uintptr_t)boot,(uintptr_t)boot+sizeof(*boot)},{(uintptr_t)raw,(uintptr_t)raw+96}},.map=&live};
 CHECK(!s842_build_pe(&q,&live,&c,&t,msr,&r));
 CHECK(s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));CHECK(s842_audit(&r,&c));
 u32(raw,0,7);CHECK(!s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));u32(raw,0,1);
 u32(raw+48,0,4);CHECK(!s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));u32(raw+48,0,2);
 u64(raw,32,0);CHECK(!s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));u64(raw,32,15);
 live.nr_reserved=1;CHECK(!s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));live.nr_reserved=2;
 ev.live.source.end++;CHECK(!s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));ev.live.source.end--;
 live.candidates[0]=q.image;live.nr_candidates=1;CHECK(!s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));live.nr_candidates=0;
 boot->memory_map_size=144;ev.live.source.end=(uintptr_t)raw+144;
 descriptor(raw+96,2,0xfee00000,1,15);CHECK(!s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));
 descriptor(raw+96,11,0xfee00000,1,1);CHECK(s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));
 descriptor(raw+96,2,q.image.start,1,15);CHECK(!s842_build_pe_efi(&q,&ev,boot,&c,&t,msr,&r));
 assert(munmap(image,(N+3u)*P)==0);
 printf("S842 PE/APIC composition: %u PASS (adjacent and separated host fixtures only)\n",n);return 0;
}
