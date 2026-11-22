#include <styleos/vtd_809.h>
#include <styleos/dmar_808.h>
#include <styleos/cache_pagewalk_801.h>
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

static uint64_t hex_read64(const volatile uint8_t *b,unsigned off){
 const volatile uint32_t *p=(const volatile uint32_t *)(b+off);
 return (uint64_t)p[0]|((uint64_t)p[1]<<32);
}
static uint32_t hex_read32(const volatile uint8_t *b,unsigned off){
 return *(const volatile uint32_t *)(b+off);
}
static void hx(uint64_t v){arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)i)&15]);}
static void fld(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');hx(v);}
static _Noreturn void die(const char *s){arch_puts("STYLEOS S809 FAIL ");arch_puts(s);arch_putc('\n');arch_halt();}

struct ctx{const struct boot_info *b;};
static bool read_table(void *user,uint64_t pa,uint16_t slot,uint64_t *result){
 const struct boot_info *b=((struct ctx *)user)->b;
 if(!result||slot>=512||!pa||(pa&4095)||pa>=UINT64_C(0x08000000)||
    !b->memory_map||b->descriptor_size<40||b->descriptor_size>4096||
    b->memory_map_size%b->descriptor_size)return false;
 const uint8_t *m=(const uint8_t *)(uintptr_t)b->memory_map;
 bool found=false;
 for(uint64_t off=0;off<b->memory_map_size;off+=b->descriptor_size){
  const uint8_t *d=m+off;uint32_t type=0;uint64_t start=0,pages=0;
  for(unsigned k=0;k<4;k++)type|=(uint32_t)d[k]<<(8u*k);
  for(unsigned k=0;k<8;k++){start|=(uint64_t)d[8+k]<<(8u*k);pages|=(uint64_t)d[24+k]<<(8u*k);}
  if(type>=11||!pages||pages>UINT64_MAX/4096u)continue;
  uint64_t len=pages*4096u;
  if(start<=pa&&pa-start<len&&len-(pa-start)>=4096u){found=true;break;}
 }
 if(!found)return false;
 *result=((const volatile uint64_t *)(uintptr_t)pa)[slot];return true;
}
/* Isolated read-only terminal test; no IOMMU writes, no DMA authorization. */
_Noreturn void vtd809_terminal(const struct boot_info *b){
 if(!b||b->magic!=STYLEOS_BOOT_MAGIC||b->flags!=STYLEOS_BOOT_EXITED_SERVICES)die("handoff");
 struct dmar808_info info={0};int observed=dmar808_get_info(&info);
 if(observed<0)die("invalid-dmar");
 arch_puts("STYLEOS S809 STATUS");fld("present",observed==1);fld("drhd-count",observed==1?info.drhd_count:0);arch_putc('\n');
 if(observed){
  if(info.drhd_count!=1||info.drhd[0].segment!=0||info.drhd[0].base==0||
     info.drhd[0].base&4095u)die("drhd-layout");
  uint64_t base=info.drhd[0].base,cr3,cr0;
  __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
  __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
  uint32_t a,bx,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(bx),"=c"(c),"=d"(d):"a"(0x80000008u),"c"(0));
  uint8_t physbits=(uint8_t)(a&255u);
  if(physbits<36||physbits>52||!(cr0&(UINT64_C(1)<<31)))die("paging-disabled");
  struct cache801_translation t={0};struct ctx context={b};
  if(!cache801_walk(cr3,base,0,physbits,read_table,&context,&t)||
     !t.present||t.physical!=base||t.page_size<4096u||
     base>UINT64_MAX-0x48u||
     (base&~(t.page_size-1u))!=((base+0x47u)&~(t.page_size-1u)))die("unmapped-drhd");
  const volatile uint8_t *mmio=(const volatile uint8_t *)(uintptr_t)base;
  struct vtd809_regs r={0};
  r.version=hex_read32(mmio,0x0);r.cap=hex_read64(mmio,0x8);r.ecap=hex_read64(mmio,0x10);
  r.gcmd=hex_read32(mmio,0x18);r.gsts=hex_read32(mmio,0x1c);
  r.root_table=hex_read64(mmio,0x20);r.fsts=hex_read32(mmio,0x34);
  if(r.version!=hex_read32(mmio,0x0)||r.cap!=hex_read64(mmio,0x8)||
     r.ecap!=hex_read64(mmio,0x10))die("unstable-static-registers");
  struct vtd809_state s={0};if(!vtd809_decode(&r,&s))die("bad-vtd-registers");
  arch_puts("STYLEOS S809 MMIO");fld("base",base);fld("mapped",1);
  fld("physical",t.physical);fld("page-size",t.page_size);fld("cr3",cr3);arch_putc('\n');
  arch_puts("STYLEOS S809 REGS");fld("ver",r.version);fld("major",s.major);fld("minor",s.minor);
  fld("cap",r.cap);fld("ecap",r.ecap);fld("gcmd",r.gcmd);fld("gsts",r.gsts);
  fld("rtaddr",r.root_table);fld("fsts",r.fsts);arch_putc('\n');
  arch_puts("STYLEOS S809 STATE");fld("translation-status",s.translation_status);
  fld("translation-command",s.translation_command);fld("root-set-status",s.root_set_status);arch_putc('\n');
 }
 arch_puts("STYLEOS S809 POLICY register-snapshot=observed dma-isolation-verified=no dma-access-authorized=no iommu-domains-verified=no uefi-runtime-verified=no production-cr3=deny\n");
 arch_puts("STYLEOS S809 PASS\n");
 __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));arch_halt();
}
