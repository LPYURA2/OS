#include <styleos/cache_policy_800.h>
#include <styleos/cache_pagewalk_801.h>
#include <styleos/desktop_display.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>

/* Isolated terminal QEMU/OVMF only. Read once from the SAME boot. No PCI BAR,
 * CR3, PAT, MTRR, page table or MMIO target writes. This is NOT a global
 * alias scan and does NOT authorize StyleOS production page tables. */
static void hx(uint64_t v){arch_puts("0x");for(int n=60;n>=0;n-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)n)&15u]);}
static void fld(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');hx(v);}
static uint64_t rdmsr(uint32_t n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return ((uint64_t)d<<32)|a;}
static uint32_t cp(uint32_t leaf,uint32_t *b,uint32_t *c,uint32_t *d){uint32_t a;__asm__ volatile("cpuid":"=a"(a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));return a;}
static uint32_t in32(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static void out32(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static uint32_t cfg(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off){out32(0xcf8u,UINT32_C(0x80000000)|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(off&0xfcu));return in32(0xcfcu);}
static _Noreturn void fail(const char *reason,uint32_t old){out32(0xcf8u,old);arch_puts("STYLEOS S803 FAIL ");arch_puts(reason);arch_putc('\n');arch_halt();}
struct walk_context{const struct boot_info *info;};
/* Inherited QEMU/OVMF root only: read a page table if the whole 4KiB
 * page lies in low, firmware-described non-MMIO RAM. This intentionally
 * inherits the narrowly tested identity mapping contract from S801. */
static bool read_table(void *ptr,uint64_t pa,uint16_t slot,uint64_t *value){
 const struct boot_info *i=((const struct walk_context *)ptr)->info;
 if(!value||slot>=512||!pa||(pa&4095u)||pa>=UINT64_C(0x08000000) || !i ||
    !i->memory_map||i->descriptor_size<40||i->descriptor_size>4096||
    !i->memory_map_size||i->memory_map_size%i->descriptor_size)return false;
 const uint8_t *map=(const uint8_t *)(uintptr_t)i->memory_map;
 bool found=false;
 for(uint64_t off=0;off<i->memory_map_size;off+=i->descriptor_size){
  const uint8_t *p=map+off;uint32_t typ=0;uint64_t base=0,count=0;
  for(unsigned n=0;n<4;n++)typ|=(uint32_t)p[n]<<(8u*n);
  for(unsigned n=0;n<8;n++){base|=(uint64_t)p[8+n]<<(8u*n);count|=(uint64_t)p[24+n]<<(8u*n);}
  if(typ>=11u||!count||count>UINT64_MAX/4096u)continue;
  uint64_t bytes=count*4096u;
  if(base<=pa && pa-base<bytes && bytes-(pa-base)>=4096u){found=true;break;}
 }
 if(!found)return false;
 *value=((const volatile uint64_t *)(uintptr_t)pa)[slot];return true;
}
_Noreturn void cache803_terminal(const struct boot_info *info,const struct desktop_surface *surface){
 uint32_t saved=in32(0xcf8u),b,c,d;
 if(!info||info->magic!=STYLEOS_BOOT_MAGIC||info->flags!=STYLEOS_BOOT_EXITED_SERVICES||
    !surface||!surface->base||!surface->bytes)fail("handoff",saved);
 if(cp(0x80000000u,&b,&c,&d)<0x80000008u)fail("cpuid",saved);
 uint8_t bits=(uint8_t)(cp(0x80000008u,&b,&c,&d)&255u);
 cp(1u,&b,&c,&d);
 if(bits<36u||bits>52u||!(d&(1u<<5))||!(d&(1u<<12))||!(d&(1u<<16)))fail("cpu-caps",saved);
 uint64_t cr0,cr3;
 __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
 __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
 if(!(cr0&(UINT64_C(1)<<31)))fail("paging-off",saved);
 const uint64_t pat=rdmsr(0x277u),cap=rdmsr(0xfeu),def=rdmsr(0x2ffu);
 const uint8_t nvar=(uint8_t)(cap&255u);
 if(!nvar||nvar>CACHE800_MAX_VAR || (def&0xc00)!=0xc00)fail("mtrr-caps",saved);
 struct cache800_pair pairs[CACHE800_MAX_VAR]={{0}};
 for(uint8_t n=0;n<nvar;n++){
  pairs[n].base=rdmsr(0x200u+2u*n);
  pairs[n].mask=rdmsr(0x201u+2u*n);
 }
 uint32_t id=cfg(1u,0u,0u,0),bar1=cfg(1u,0u,0u,0x14u),bar4=cfg(1u,0u,0u,0x20u),bar5=cfg(1u,0u,0u,0x24u);
 out32(0xcf8u,saved);
 if(id!=UINT32_C(0x10431af4)||(bar1&15u)||(bar4&15u)!=12u)fail("pci",saved);
 uint64_t addresses[3]={(uint64_t)surface->base,(uint64_t)(bar1&~15u),((uint64_t)bar5<<32)|(uint64_t)(bar4&~15u)};
 if(addresses[0]!=UINT64_C(0x80000000)||addresses[1]!=UINT64_C(0x81000000)||addresses[2]!=UINT64_C(0xc000000000))fail("addresses",saved);
 arch_puts("STYLEOS S803 CPU");fld("cr0",cr0);fld("cr3",cr3);fld("pat",pat);fld("mtrrcap",cap);fld("mtrrdef",def);fld("physbits",bits);fld("variable-count",nvar);arch_putc('\n');
 arch_puts("STYLEOS S803 PCI");fld("id",id);fld("gop",addresses[0]);fld("bar1raw",bar1);fld("bar4raw",bar4);fld("bar5raw",bar5);arch_putc('\n');
 for(uint8_t n=0;n<nvar;n++){
  arch_puts("STYLEOS S803 MTRR");fld("index",n);fld("base",pairs[n].base);fld("mask",pairs[n].mask);arch_putc('\n');
 }
 struct walk_context ctx={info};
 for(unsigned n=0;n<3;n++){
  struct cache800_result m={0};struct cache801_translation t={0};
  if(!cache800_physical_type(addresses[n],bits,cap,def,pairs,nvar,&m)||
     !cache801_walk(cr3,addresses[n],pat,bits,read_table,&ctx,&t))fail("model",saved);
  /* A PAT+MTRR derived type is asserted only for the sampled, identity-
   * translated, UC physical address, not for any other alias or CPU mode. */
  if(!t.present||t.physical!=addresses[n]||!m.mtrr_enabled||m.type!=0u)fail("unverified-sample",saved);
  uint8_t effective=0u; /* architectural UC dominates PAT on this sample */
  arch_puts("STYLEOS S803 TARGET");fld("index",n);fld("address",addresses[n]);
  fld("mtrr-type",m.type);fld("matches",m.matched);fld("enabled",m.mtrr_enabled);
  fld("present",t.present);fld("translated",t.physical);fld("page-size",t.page_size);
  fld("entry",t.leaf_entry);fld("pat-index",t.pat_index);fld("pat-type",t.pat_type);
  fld("effective-sample-type",effective);arch_putc('\n');
 }
 arch_puts("STYLEOS S803 POLICY single-boot-samples=yes global-alias-scan=unverified all-mmio-cache=unverified irq-apic=unverified dma-iommu=unverified firmware-runtime=unverified production-cr3=deny\n");
 arch_puts("STYLEOS S803 PASS\n");out32(0xf4u,0x10u);arch_halt();
}
