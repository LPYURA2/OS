#include <styleos/owned_low_790.h>
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
int main(void)
{
    image=mmap(NULL,N*P,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    stackbuf=mmap(NULL,2u*P,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    assert(image!=MAP_FAILED && stackbuf!=MAP_FAILED);
    pe();
    struct s790_low_request r=request();
    struct s790_low_result low={0};
    assert(s790_prepare_owned_low(&r,&low));
    assert(low.ready && low.code_pages==2 && low.readonly_pages==2 &&
           low.writable_pages==36 && low.stack_pages==2 && low.built.used<=PG_TABLE_PAGES);
    assert(s790_audit_owned_low(&low,r.required_exec,r.required_write,r.required_rsp));
    /* Real QEMU OVMF entry stack is embedded in loaded PE .data/.bss.
     * Its pages must be reused without a duplicate VA mapping. */
    struct s790_low_request embedded=request();
    embedded.boot_stack=(struct boot_range){
        (uint64_t)(uintptr_t)(image+10u*P+16u),
        (uint64_t)(uintptr_t)(image+12u*P+16u)};
    embedded.required_rsp=(uint64_t)(uintptr_t)(image+11u*P+24u);
    assert(s790_prepare_owned_low(&embedded,&low));
    assert(low.stack_pages==3 && low.plan.count==4 &&
           s790_audit_owned_low(&low,embedded.required_exec,
                                embedded.required_write,embedded.required_rsp));
    embedded.boot_stack=(struct boot_range){
        (uint64_t)(uintptr_t)(image+P+16u),
        (uint64_t)(uintptr_t)(image+2u*P+16u)};
    embedded.required_rsp=(uint64_t)(uintptr_t)(image+P+32u);
    assert(!s790_prepare_owned_low(&embedded,&low)); /* RX stack forbidden */
    r=request();assert(s790_prepare_owned_low(&r,&low));
    low.plan.tables[0][0]^=2u;
    assert(!s790_audit_owned_low(&low,r.required_exec,r.required_write,r.required_rsp));
    low.plan.tables[0][0]^=2u;
    assert(s790_audit_owned_low(&low,r.required_exec,r.required_write,r.required_rsp));
    r.required_exec=r.required_write;
    assert(!s790_prepare_owned_low(&r,&low) && !low.ready);
    r=request();r.boot_stack.start=r.image.start;
    assert(!s790_prepare_owned_low(&r,&low) && !low.ready);
    r=request();r.owned_tables_pa+=P;
    assert(!s790_prepare_owned_low(&r,&low) && !low.ready);
    r=request();r.required_rsp=0;
    assert(!s790_prepare_owned_low(&r,&low) && !low.ready);
    r=request();u32(image,0x80+24+56,42u*P);
    assert(!s790_prepare_owned_low(&r,&low) && !low.ready);
    pe();u32(image,0x80+24+0xf0+36,0xe0000000u);
    assert(!s790_prepare_owned_low(&r,&low) && !low.ready);
    pe();u32(image,0x80+24+0xf0+8,0);
    assert(!s790_prepare_owned_low(&r,&low) && !low.ready);
    pe();r.loaded_image=(const void*)(image+P);
    assert(!s790_prepare_owned_low(&r,&low) && !low.ready);
    puts("STYLEOS 790 OWNED LOW HOST PASS: PE roles, NX, root, refusals, sanitizer");
    return 0;
}
