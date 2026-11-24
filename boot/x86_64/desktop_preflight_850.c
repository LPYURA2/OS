/* Terminal QEMU standard-VGA diagnostic, after EBS and before MOV CR3.
 * Not called by Desktop or a live display driver. No MMIO access. */
#include <styleos/desktop_root_850.h>
#include <styleos/arch.h>
#include <styleos/ist_guard_plan_785.h>
static _Alignas(4096) uint64_t tables[PG_TABLE_PAGES][512];
static struct s850_desktop_root composed;
static uint64_t read_cr3(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static uint64_t flags(void){uint64_t v;__asm__ volatile("pushfq;popq %0":"=r"(v));return v;}
static uint32_t in32(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static uint16_t in16(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static void out32(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p):"memory");}
static void out16(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p):"memory");}
static uint32_t cfgaddr(unsigned dev,unsigned fn,unsigned off){return UINT32_C(0x80000000)|(dev<<11)|(fn<<8)|(off&0xfcu);}
static uint32_t cfg(unsigned dev,unsigned fn,unsigned off){out32(0xcf8,cfgaddr(dev,fn,off));return in32(0xcfc);}
static void cfgwrite(unsigned dev,unsigned fn,unsigned off,uint32_t v){out32(0xcf8,cfgaddr(dev,fn,off));out32(0xcfc,v);}
static uint16_t cmd(unsigned dev,unsigned fn){out32(0xcf8,cfgaddr(dev,fn,4));return in16(0xcfc);}
static void cmdwrite(unsigned dev,unsigned fn,uint16_t v){out32(0xcf8,cfgaddr(dev,fn,4));out16(0xcfc,v);}
static void field(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');paging_hex(v);}
static _Noreturn void fail(const char *s){arch_puts("STYLEOS S850 FAIL ");arch_puts(s);arch_putc('\n');out32(0xf4,0x11);arch_halt();}
/* Same narrow reversible S795 probe, restricted further to the standard
 * QEMU VGA fixture. No probe on an unknown device or driver-owned BAR.
 * Restore the command, BAR, and CF8 on every path following a write. */
static bool capture(const struct desktop_surface *s,struct pci794_bar *out,uint32_t *mask,unsigned *dev_out,unsigned *fn_out,uint16_t *command_out){
 uint32_t old=in32(0xcf8);unsigned matches=0,dev=0,fn=0;struct pci794_bar bar={0};struct pci794_device device={0};
 for(unsigned d=0;d<32;d++)for(unsigned f=0;f<8;f++){
  uint32_t words[16];words[0]=cfg(d,f,0);if((words[0]&0xffff)==0xffff)continue;
  for(unsigned i=1;i<16;i++)words[i]=cfg(d,f,4*i);
  struct pci794_device candidate;struct pci794_bar b;
  if(pci794_parse_header(words,&candidate)&&pci794_find_framebuffer_bar(&candidate,s->base,&b)){matches++;dev=d;fn=f;bar=b;device=candidate;}
 }
 bool ok=false;uint32_t size_mask=0;uint16_t before=0;
 if(matches==1&&device.vendor==0x1234&&device.device==0x1111&&device.class_code==3&&device.subclass==0&&bar.index==0&&bar.width==32&&bar.memory&&bar.base==s->base){
  uint32_t low=cfg(dev,fn,0x10);before=cmd(dev,fn);
  if(low==bar.raw_lo&&(before&2u)){
   cmdwrite(dev,fn,(uint16_t)(before&~7u));bool disabled=(cmd(dev,fn)&7u)==0;
   if(disabled){cfgwrite(dev,fn,0x10,UINT32_MAX);size_mask=cfg(dev,fn,0x10);}
   cfgwrite(dev,fn,0x10,low);cmdwrite(dev,fn,before);
   bool restored=cfg(dev,fn,0x10)==low&&cmd(dev,fn)==before;
   struct pci795_aperture aperture;
   ok=disabled&&restored&&pci795_decode_aperture(&bar,size_mask,0,s->base,s->bytes,&aperture);
  }
 }
 out32(0xcf8,old);if(in32(0xcf8)!=old||!ok)return false;
 *out=bar;*mask=size_mask;*dev_out=dev;*fn_out=fn;*command_out=before;return true;
}
extern bool s850_read_inherited_entry(void *,uint64_t,uint16_t,uint64_t *);
_Noreturn void s850_preflight(const struct s842_root *base,const struct s842_cache *cache,struct pg_experiment *p,const struct s846_guard *guard,uint64_t gdt,uint64_t idt,uint64_t tss,uint64_t rsp0){
 if(!p||!p->evidence||!guard||read_cr3()!=p->old_root||(flags()&0x200)||arch_paging_attest_image_writable(p,(uintptr_t)tables,sizeof tables)!=PG_OK)fail("context-storage");
 const struct desktop_surface *surface=desktop_platform_surface();
 if(!desktop_surface_valid(surface))fail("surface-invalid");
 if(!desktop_surface_excludes_boot_memory(surface,p->evidence))fail("surface-overlap");
 const struct boot_info *boot=(const void *)(uintptr_t)p->evidence->live.handoff.start;
 struct pci794_bar bar;uint32_t mask;unsigned dev,fn;uint16_t command;
 if(!capture(surface,&bar,&mask,&dev,&fn,&command))fail("vga-bar");
 struct s850_gop_request req={boot,p->evidence,surface,&bar,mask,0,cache,p->old_root,s850_read_inherited_entry,p};
 if(!s850_desktop_build_guarded(base,&req,guard,tables,(uintptr_t)tables,&composed)||!s850_desktop_audit_guarded(base,&req,guard,&composed))fail("tree");
 const uint64_t desc[]={gdt,idt,tss,rsp0};
 for(unsigned i=0;i<4;i++){
  struct pg_lookup x;
  if(arch_paging_attest_image_writable(p,desc[i],4096)!=PG_OK||pg_lookup(&composed.plan,desc[i],&x)!=PG_OK||x.pa!=desc[i]||x.flags!=PG_WRITE)fail("descriptor-closure");
 }
 if(read_cr3()!=p->old_root||(flags()&0x200))fail("root-changed");
 arch_puts("STYLEOS S850 GOP");field("base",surface->base);field("bytes",surface->bytes);field("width",surface->width);field("height",surface->height);field("stride",surface->stride);field("format",surface->format);arch_putc('\n');
 arch_puts("STYLEOS S850 BAR");field("dev",dev);field("fn",fn);field("command",command);field("raw",bar.raw_lo);field("mask",mask);field("size",composed.gop.bar_bytes);arch_putc('\n');
 arch_puts("STYLEOS S850 ROOT");field("inherited",p->old_root);field("live",read_cr3());field("detached",composed.built.root);field("source",base->built.root);field("used",composed.built.used);field("pages",composed.gop.pages);field("pat-index",composed.gop.pat_index);field("cache-bits",composed.gop.cache_bits);field("pat",cache->pat);arch_putc('\n');
 arch_puts("STYLEOS S850 DESCRIPTORS");field("gdt",gdt);field("idt",idt);field("tss",tss);field("rsp0",rsp0);field("ist1",guard->pa[0]);field("ist2",guard->pa[1]);field("ist3",guard->pa[2]);arch_putc('\n');
 arch_puts("STYLEOS S850 POLICY bar-restored=yes detached-audited=yes descriptors-staged=yes desktop-root-active=no framebuffer-write=no production-cr3=deny\n");out32(0xf4,0x10);arch_halt();
}
