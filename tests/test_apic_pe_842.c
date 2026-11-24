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
int main(void){
 image=mmap(NULL,N*P,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 stackbuf=mmap(NULL,2u*P,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 assert(image!=MAP_FAILED&&stackbuf!=MAP_FAILED);pe();
 struct s790_low_request p=request();
 struct boot_memory_map m={.ram={p.image,p.boot_stack},.reserved={p.image,p.boot_stack},.nr_ram=2,.nr_reserved=2};
 struct s842_cache c={.pat=0x0007040600070406,.cap=1,.def=0x806,.count=1,.bits=48,.pairs={{0x80000000,0xffff80000800}}};
 struct acpi806_topology t={.enabled_lapics=1,.ioapic_count=1,.lapic_physical=0xfee00000,.ioapic={{0,0xfec00000,0}}};
 struct s842_root r;uint64_t msr=0xfee00900;unsigned n=0;
 #define CHECK(x) do{assert(x);n++;}while(0)
 CHECK(s842_build_pe(&p,&m,&c,&t,msr,&r));CHECK(s842_audit(&r,&c));
 CHECK(r.plan.count==7);CHECK(r.lapic==t.lapic_physical&&r.ioapic==t.ioapic[0].physical);
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
 printf("S842 PE/APIC composition: %u PASS (host fixture only)\n",n);return 0;
}
