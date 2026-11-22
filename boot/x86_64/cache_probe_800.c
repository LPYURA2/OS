#include <styleos/cache_policy_800.h>
#include <styleos/desktop_display.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
static void hx(uint64_t v){arch_puts("0x");for(int n=60;n>=0;n-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)n)&15u]);}
static void fld(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');hx(v);}
static uint64_t rdmsr(uint32_t n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return ((uint64_t)d<<32)|a;}
static uint32_t cp(uint32_t leaf,uint32_t *b,uint32_t *c,uint32_t *d){uint32_t a;__asm__ volatile("cpuid":"=a"(a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));return a;}
static uint32_t in32(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static void out32(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static uint32_t cfg(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off){out32(0xcf8u,UINT32_C(0x80000000)|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(off&0xfcu));return in32(0xcfcu);}
static _Noreturn void fail(const char *s,uint32_t cf8){out32(0xcf8u,cf8);arch_puts("STYLEOS S800 FAIL ");arch_puts(s);arch_putc('\n');arch_halt();}
/* TERMINAL, read-only experiment, no CR3/PAT/MTRR writes. This does NOT
 * prove effective PAT selection of PCI MMIO because a PTE and overlap
 * policy are needed. The inherited UEFI root remains intact. */
_Noreturn void cache800_terminal(const struct boot_info *info,const struct desktop_surface *surface)
{
    uint32_t oldcf8=in32(0xcf8u),b,c,d;
    if(!info || info->magic!=STYLEOS_BOOT_MAGIC || info->flags!=STYLEOS_BOOT_EXITED_SERVICES ||
       !surface || !surface->base || !surface->bytes)fail("handoff",oldcf8);
    uint32_t maxext=cp(0x80000000u,&b,&c,&d);
    if(maxext<0x80000008u)fail("physical-address-cpuid",oldcf8);
    uint32_t phys=cp(0x80000008u,&b,&c,&d)&255u;
    cp(1u,&b,&c,&d);
    if(!(d&(1u<<5)) || !(d&(1u<<12)) || !(d&(1u<<16)) || phys<36u || phys>52u)fail("mtrr-msr-caps",oldcf8);
    uint64_t cr0,cr3;__asm__ volatile("mov %%cr0,%0":"=r"(cr0));__asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    uint64_t pat=rdmsr(0x277u),cap=rdmsr(0xfeu),def=rdmsr(0x2ffu);
    uint8_t count=(uint8_t)(cap&255u);
    if(!count || count>CACHE800_MAX_VAR)fail("mtrr-count",oldcf8);
    struct cache800_pair pairs[CACHE800_MAX_VAR]={{0}};
    for(uint8_t i=0;i<count;i++){
        pairs[i].base=rdmsr(0x200u+2u*i);
        pairs[i].mask=rdmsr(0x201u+2u*i);
    }
    uint32_t id=cfg(1,0,0,0),raw1=cfg(1,0,0,0x14),raw4=cfg(1,0,0,0x20),raw5=cfg(1,0,0,0x24);
    out32(0xcf8u,oldcf8);
    if(id!=UINT32_C(0x10431af4) || (raw1&15u)!=0 || (raw4&15u)!=12u ||
       !surface->base || (surface->base&4095u))fail("pci-endpoint-or-gop",oldcf8);
    uint64_t addresses[3]={(uint64_t)surface->base,(uint64_t)(raw1&~15u),
      ((uint64_t)raw5<<32)|(uint64_t)(raw4&~15u)};
    if(addresses[1]==0 || addresses[2]==0 || addresses[0]==addresses[1] || addresses[1]==addresses[2])fail("bar-bases",oldcf8);
    arch_puts("STYLEOS S800 CPU");fld("cr0",cr0);fld("cr3",cr3);fld("pat",pat);
    fld("mtrrcap",cap);fld("mtrrdef",def);fld("physbits",phys);fld("variable-count",count);arch_putc('\n');
    arch_puts("STYLEOS S800 PCI");fld("id",id);fld("gop",addresses[0]);
    fld("bar1raw",raw1);fld("bar4raw",raw4);fld("bar5raw",raw5);arch_putc('\n');
    for(uint8_t i=0;i<count;i++){
        arch_puts("STYLEOS S800 MTRR");fld("index",i);fld("base",pairs[i].base);fld("mask",pairs[i].mask);arch_putc('\n');
    }
    for(uint8_t i=0;i<3u;i++){
        struct cache800_result r={0};
        if(!cache800_physical_type(addresses[i],(uint8_t)phys,cap,def,pairs,count,&r))fail("physical-mtrr-model",oldcf8);
        arch_puts("STYLEOS S800 TARGET");fld("index",i);fld("address",addresses[i]);
        fld("mtrr-type",r.type);fld("matches",r.matched);fld("enabled",r.mtrr_enabled);arch_putc('\n');
    }
    arch_puts("STYLEOS S800 POLICY physical-mtrr-observed=yes pat-raw-observed=yes effective-pat-pte=unknown irq-apic=unverified dma-iommu=unverified firmware-runtime=unverified production-cr3=deny\n");
    arch_puts("STYLEOS S800 PASS\n");
    out32(0xf4u,0x10u);arch_halt();
}
