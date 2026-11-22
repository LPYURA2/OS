#include <styleos/cache_pagewalk_801.h>
#include <styleos/desktop_display.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
static void hx(uint64_t v){arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)i)&15u]);}
static void f(const char *name,uint64_t v){arch_putc(' ');arch_puts(name);arch_putc('=');hx(v);}
static uint64_t rdmsr(uint32_t n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return ((uint64_t)d<<32)|a;}
static uint32_t cp(uint32_t leaf,uint32_t *b,uint32_t *c,uint32_t *d){uint32_t a;__asm__ volatile("cpuid":"=a"(a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));return a;}
static uint32_t in32(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static void out32(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static uint32_t cfg(uint8_t bus,uint8_t dev,uint8_t off){out32(0xcf8,UINT32_C(0x80000000)|((uint32_t)bus<<16)|((uint32_t)dev<<11)|(off&0xfcu));return in32(0xcfcu);}
static _Noreturn void fail(const char *s,uint32_t old){out32(0xcf8u,old);arch_puts("STYLEOS S801 FAIL ");arch_puts(s);arch_putc('\n');arch_halt();}
struct context {const struct boot_info *info;};
/* UEFI/QEMU-only: page-table physical pages must be in low RAM with a
 * UEFI descriptor and identity mapped by inherited firmware CR3. A range
 * beyond 128MiB, MMIO, or a truncated descriptor fails closed. */
static bool safe_table(void *ptr,uint64_t pa,uint16_t index,uint64_t *value){
    const struct context *ctx=ptr;const struct boot_info *i=ctx->info;
    if(!value || index>=512 || !pa || (pa&4095u) || pa>=UINT64_C(0x08000000))return false;
    if(!i->memory_map || i->descriptor_size<40 || i->descriptor_size>4096 ||
       !i->memory_map_size || i->memory_map_size%i->descriptor_size)return false;
    const uint8_t *map=(const uint8_t *)(uintptr_t)i->memory_map;
    bool found=false;
    for(uint64_t off=0;off<i->memory_map_size;off+=i->descriptor_size){
        const uint8_t *p=map+off;
        uint32_t typ=0;uint64_t start=0,pages=0;
        for(unsigned n=0;n<4;n++)typ|=(uint32_t)p[n]<<(8u*n);
        for(unsigned n=0;n<8;n++){start|=(uint64_t)p[8+n]<<(8u*n);pages|=(uint64_t)p[24+n]<<(8u*n);}
        if(typ>=11u || !pages || pages>UINT64_MAX/4096u)continue;
        uint64_t bytes=pages*4096u;
        if(start<=pa && pa-start<bytes && bytes-(pa-start)>=4096u){found=true;break;}
    }
    if(!found)return false;
    const volatile uint64_t *table=(const volatile uint64_t *)(uintptr_t)pa;
    *value=table[index];return true;
}
/* Terminal probe: never changes PAT/MTRR, CR3, PCI BAR or mapping flags.
 * Does not execute or access target MMIO. Does not establish alias absence. */
_Noreturn void cache801_terminal(const struct boot_info *info,const struct desktop_surface *surface){
    uint32_t saved=in32(0xcf8u),b,c,d;
    if(!info || info->magic!=STYLEOS_BOOT_MAGIC || info->flags!=STYLEOS_BOOT_EXITED_SERVICES ||
       !surface || !surface->base || !surface->bytes)fail("boot",saved);
    if(cp(0x80000000u,&b,&c,&d)<0x80000008u)fail("cpuid",saved);
    uint8_t physical_bits=(uint8_t)(cp(0x80000008u,&b,&c,&d)&255u);
    uint64_t cr3,cr0;__asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    if(physical_bits<36 || physical_bits>52 || !(cr0&(UINT64_C(1)<<31)))fail("cpu",saved);
    uint64_t pat=rdmsr(0x277u);
    uint32_t id=cfg(1u,0u,0u),raw1=cfg(1u,0u,0x14),raw4=cfg(1u,0u,0x20),raw5=cfg(1u,0u,0x24);
    out32(0xcf8u,saved);
    if(id!=UINT32_C(0x10431af4) || (raw1&15u) || (raw4&15u)!=12u)fail("device",saved);
    uint64_t addresses[3]={(uint64_t)surface->base,(uint64_t)(raw1&~15u),
      ((uint64_t)raw5<<32)|(uint64_t)(raw4&~15u)};
    if(addresses[0]!=UINT64_C(0x80000000) || addresses[1]!=UINT64_C(0x81000000) ||
       addresses[2]!=UINT64_C(0xc000000000))fail("targets",saved);
    arch_puts("STYLEOS S801 CPU");f("cr0",cr0);f("cr3",cr3);f("pat",pat);
    f("physbits",physical_bits);arch_putc('\n');
    struct context ctx={info};
    unsigned mapped=0;
    for(unsigned n=0;n<3;n++){
        struct cache801_translation t={0};
        if(!cache801_walk(cr3,addresses[n],pat,physical_bits,safe_table,&ctx,&t))fail("walk-inaccessible",saved);
        if(t.present)++mapped;
        arch_puts("STYLEOS S801 TARGET");f("index",n);f("address",addresses[n]);
        f("present",t.present);f("translated",t.physical);f("page-size",t.page_size);
        f("entry",t.leaf_entry);f("pat-index",t.pat_index);f("pat-type",t.pat_type);arch_putc('\n');
    }
    if(mapped==0u)fail("no-sampled-mappings",saved);
    arch_puts("STYLEOS S801 POLICY live-pagewalk=observed global-alias-scan=unverified effective-mmio-cache=unverified production-cr3=deny\n");
    arch_puts("STYLEOS S801 PASS\n");out32(0xf4u,0x10u);arch_halt();
}
