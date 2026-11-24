/* Terminal QEMU standard-VGA diagnostic, after EBS and before MOV CR3.
 * Not called by Desktop or a live display driver. No MMIO access. */
#include <styleos/desktop_root_850.h>
#include <styleos/arch.h>
#include <styleos/ist_guard_plan_785.h>
#ifdef STYLEOS_TEST_DESKTOP_FRAME_852
#include <styleos/desktop_v0.h>
#endif
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
#ifdef STYLEOS_TEST_DESKTOP_SWITCH_851
/* Terminal QEMU-only experiment. Transition IDT has no IST dependency and
 * uses the common CPL0 stack until the new TSS is loaded. It never returns
 * from an unexpected exception, and does not claim general NMI/DF recovery. */
struct s851_dt {uint16_t limit;uint64_t base;} __attribute__((packed));
struct s851_gate {uint16_t lo,cs;uint8_t ist,attr;uint16_t mid;uint32_t hi,zero;} __attribute__((packed));
_Static_assert(sizeof(struct s851_gate)==16,"IDT gate");
static _Alignas(4096) struct s851_gate bridge[256];
volatile uint64_t s851_frame[5],s851_frame_address;
void irq807_unexpected(void);void s851_bp(void);
__asm__(".text\n.global s851_bp\ns851_bp:\n"
 "pushq %rax\nleaq 8(%rsp),%rax\nmovq %rax,s851_frame_address(%rip)\n"
 "movq 8(%rsp),%rax\nmovq %rax,s851_frame(%rip)\n"
 "movq 16(%rsp),%rax\nmovq %rax,s851_frame+8(%rip)\n"
 "movq 24(%rsp),%rax\nmovq %rax,s851_frame+16(%rip)\n"
 "movq 32(%rsp),%rax\nmovq %rax,s851_frame+24(%rip)\n"
 "movq 40(%rsp),%rax\nmovq %rax,s851_frame+32(%rip)\n"
 "popq %rax\niretq\n");
static struct s851_gate make_gate(uint64_t fn,uint16_t cs,uint8_t ist){return (struct s851_gate){(uint16_t)fn,cs,ist,0x8e,(uint16_t)(fn>>16),(uint32_t)(fn>>32),0};}
static uint64_t reg4(void){uint64_t v;__asm__ volatile("mov %%cr4,%0":"=r"(v));return v;}
static uint64_t read_msr(uint32_t n){uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(n));return ((uint64_t)hi<<32)|lo;}
static bool same_cache(const struct s842_cache *c){if(read_msr(0x277)!=c->pat||read_msr(0xfe)!=c->cap||read_msr(0x2ff)!=c->def)return false;for(unsigned i=0;i<c->count;i++)if(read_msr(0x200+2*i)!=c->pairs[i].base||read_msr(0x201+2*i)!=c->pairs[i].mask)return false;return true;}
static bool identity(uint64_t va,unsigned rights){struct pg_lookup x;return pg_lookup(&composed.plan,va,&x)==PG_OK&&x.pa==va&&x.flags==rights;}
static bool absent(uint64_t va){struct pg_lookup x;return pg_lookup(&composed.plan,va,&x)==PG_RANGE;}
static uint64_t bytes64(const unsigned char *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static _Noreturn void s851_switch(struct pg_experiment *p,const struct s842_cache *cache,const struct s846_guard *guard,const struct desktop_surface *s,uint64_t gdt,uint64_t idt,uint64_t tss,uint64_t rsp0,uint16_t limit,uint16_t selector){
 uint16_t cs,tr;uint64_t rsp,cr0;__asm__ volatile("mov %%cs,%0;str %1;mov %%rsp,%2;mov %%cr0,%3":"=r"(cs),"=r"(tr),"=r"(rsp),"=r"(cr0));
 uint64_t cr4=reg4(),target=composed.built.root,bridge_pa=(uintptr_t)bridge;
 if(!cs||(cs&7)||tr||flags()&0x200||!(cr0&(UINT64_C(1)<<16))||!(read_msr(0xc0000080)&(UINT64_C(1)<<11))||cr4&((UINT64_C(1)<<12)|(UINT64_C(1)<<17))||read_cr3()!=p->old_root||target==p->old_root||!same_cache(cache)||!selector||(selector&7)||limit!=selector+15||limit>=4096||cs+7>limit)fail("851-mode");
 if(arch_paging_attest_image_writable(p,bridge_pa,sizeof bridge)!=PG_OK||!identity(bridge_pa,PG_WRITE)||!identity((uintptr_t)s851_bp,PG_EXEC)||!identity((uintptr_t)irq807_unexpected,PG_EXEC)||!identity(rsp,PG_WRITE)||!identity((uintptr_t)s851_frame,PG_WRITE)||!identity((uintptr_t)&s851_frame_address,PG_WRITE))fail("851-bridge-closure");
 const unsigned char *ts=(const void *)(uintptr_t)tss,*gd=(const void *)(uintptr_t)gdt;uint64_t lo=bytes64(gd+selector),hi=bytes64(gd+selector+8);
 if(((lo>>40)&255)!=0x89||((lo&0xffff)|(((lo>>48)&15)<<16))!=103||((lo>>16&0xffffff)|(lo>>56<<24)|(hi<<32))!=tss||hi>>16||bytes64(ts+4)!=rsp0+4096-16)fail("851-descriptor");
 for(unsigned i=0;i<3;i++){uint64_t va=S846_IST+3*i*4096;struct pg_lookup x;if(bytes64(ts+36+8*i)!=va+4096-16||pg_lookup(&composed.plan,va,&x)!=PG_OK||x.pa!=guard->pa[i]||x.flags!=PG_WRITE||!absent(va-4096)||!absent(va+4096))fail("851-ist");}
 for(unsigned i=0;i<256;i++)bridge[i]=make_gate((uintptr_t)irq807_unexpected,cs,0);
 ((struct s851_gate *)(uintptr_t)idt)[3]=make_gate((uintptr_t)s851_bp,cs,1);
 struct s851_dt b={4095,bridge_pa},g={limit,gdt},d={4095,idt},live_g,live_d;
 arch_puts("STYLEOS S851 BRIDGE");field("idt",bridge_pa);field("old",p->old_root);field("new",target);field("cs",cs);field("selector",selector);arch_putc('\n');
 /* IDT with IST=0 first. Any unexpected vector exits on the common stack;
  * after new CR3 the new GDT and bridge remain valid, then LTR and final IDT. */
 __asm__ volatile("cli;lidt %0;lgdt %1;mov %2,%%cr4;mov %3,%%cr3;ltr %4;lidt %5"::"m"(b),"m"(g),"r"(cr4&~(UINT64_C(1)<<7)),"r"(target),"rm"(selector),"m"(d):"memory");
 __asm__ volatile("sgdt %0;sidt %1;str %2":"=m"(live_g),"=m"(live_d),"=r"(tr)::"memory");
 if(read_cr3()!=target||reg4()&(UINT64_C(1)<<7)||flags()&0x200||live_g.base!=gdt||live_g.limit!=limit||live_d.base!=idt||live_d.limit!=4095||tr!=selector||((volatile const unsigned char *)gd)[selector+5]!=0x8b||!same_cache(cache))fail("851-live-descriptors");
 arch_puts("STYLEOS S851 ACTIVE");field("root",read_cr3());field("gdt",live_g.base);field("idt",live_d.base);field("tr",tr);field("pat",read_msr(0x277));field("pge",(reg4()>>7)&1);arch_putc('\n');
 uint64_t expected_pc,before_rsp;__asm__ volatile("leaq s851_bp_return(%%rip),%0;mov %%rsp,%1;int3;.global s851_bp_return;s851_bp_return:":"=&r"(expected_pc),"=&r"(before_rsp)::"memory");
 if(read_cr3()!=target||s851_frame_address!=S846_IST+4096-16-40||s851_frame[0]!=expected_pc||s851_frame[1]!=cs||s851_frame[2]&0x200||s851_frame[3]!=before_rsp)fail("851-real-ist");
 arch_puts("STYLEOS S851 BP");field("root",read_cr3());field("frame",s851_frame_address);field("rip",s851_frame[0]);field("rsp",s851_frame[3]);field("cs",s851_frame[1]);field("flags",s851_frame[2]);arch_putc('\n');
 uint64_t offsets[2]={0,4*((uint64_t)(s->height-1)*s->stride+s->width-1)};
 for(unsigned i=0;i<2;i++){
  if(offsets[i]>composed.gop.mapping.bytes-4||offsets[i]>s->bytes-4)fail("851-pixel-bounds");
  volatile uint32_t *pixel=(void *)(s->base+offsets[i]);uint32_t old=*pixel,want=old^0x00ffffffu;
  *pixel=want;__asm__ volatile("mfence":::"memory");uint32_t got=*pixel;
  *pixel=old;__asm__ volatile("mfence":::"memory");uint32_t restored=*pixel;
  if(got!=want||restored!=old||read_cr3()!=target)fail("851-pixel");
  arch_puts("STYLEOS S851 PIXEL");field("id",i);field("root",target);field("address",(uintptr_t)pixel);field("old",old);field("write",want);field("read",got);field("restored",restored);arch_putc('\n');
 }
 arch_puts("STYLEOS S851 POLICY terminal=yes own-root=yes hardware-ist=yes framebuffer-restored=yes firmware-return=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_DESKTOP_FRAME_852
 /* Bound the diagnostic workload to the admitted standard VGA mode. */
 if(s->width!=1280||s->height!=800||s->stride!=1280||s->format!=DESKTOP_BGR_BYTES||
    s->bytes!=UINT64_C(4096000)||composed.gop.mapping.bytes<s->bytes||
    !identity((uintptr_t)desk_first_frame_852,PG_EXEC)||read_cr3()!=target)fail("852-frame-contract");
 size_t writes=desk_first_frame_852(s);
 if(writes!=(size_t)s->width*s->height||read_cr3()!=target||flags()&0x200)fail("852-frame-write");
 __asm__ volatile("mfence":::"memory");
 const volatile uint32_t *fb=(const volatile uint32_t *)s->base;
 uint64_t hash=UINT64_C(14695981039346656037);
 for(size_t i=0;i<writes;i++){
    uint32_t pixel=fb[i];
    for(unsigned b=0;b<4;b++){hash^=(pixel>>(8*b))&255u;hash*=UINT64_C(1099511628211);}
 }
 if(read_cr3()!=target||flags()&0x200||!same_cache(cache))fail("852-frame-readback");
 arch_puts("STYLEOS S852 FRAME");field("root",target);field("width",s->width);field("height",s->height);field("writes",writes);field("fnv",hash);field("first",fb[0]);field("last",fb[writes-1]);arch_putc('\n');
 arch_puts("STYLEOS S852 POLICY first-frame=yes input=no storage=no app-admission=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_DESKTOP_INPUT_853
 /* This is a fixed Q35/TCG terminal fixture, not general ACPI resource admission. */
 uint32_t a,b,c,d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(1),"2"(0));
 if(!(c&(1u<<31)))fail("853-hypervisor");
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(0x40000000),"2"(0));
 if(b!=0x54474354u||c!=0x43544743u||d!=0x47435447u)fail("853-tcg");
 uint32_t cf8=in32(0xcf8),lpc=cfg(31,0,0),classreg=cfg(31,0,8);
 out32(0xcf8,cf8);
 if(in32(0xcf8)!=cf8||lpc!=0x29188086u||(classreg>>16)!=0x0601||
    !identity((uintptr_t)desk_input_init,PG_EXEC)||!identity((uintptr_t)desk_input_next,PG_EXEC)||
    !identity((uintptr_t)desk_cursor_853,PG_EXEC)||read_cr3()!=target||flags()&0x200)
     fail("853-fixture");
 unsigned input=desk_input_init();
 if(input!=3||read_cr3()!=target||flags()&0x200)fail("853-init");
 arch_puts("STYLEOS S853 READY");field("root",target);field("lpc",lpc);field("input",input);arch_putc('\n');
 struct desk_event event={0};bool key=false,mouse=false;
 for(unsigned i=0;i<20000000u;i++){
    if(!desk_input_next(&event)){__asm__ volatile("pause");continue;}
    if(event.mouse||event.key!=0x1e||event.modifiers)fail("853-key");
    key=true;break;
 }
 if(!key||read_cr3()!=target||flags()&0x200)fail("853-key-timeout");
 arch_puts("STYLEOS S853 KEY");field("root",target);field("key",event.key);field("modifiers",event.modifiers);arch_putc('\n');
 for(unsigned i=0;i<20000000u;i++){
    if(!desk_input_next(&event)){__asm__ volatile("pause");continue;}
    if(!event.mouse||event.dx!=20||event.dy!=0||event.buttons||event.modifiers)fail("853-mouse");
    mouse=true;break;
 }
 if(!mouse||read_cr3()!=target||flags()&0x200)fail("853-mouse-timeout");
 size_t moved=desk_cursor_853(&event);
 if(!moved||moved>340||read_cr3()!=target||flags()&0x200)fail("853-damage");
 __asm__ volatile("mfence":::"memory");
 uint64_t after=UINT64_C(14695981039346656037);
 for(size_t i=0;i<writes;i++){uint32_t pixel=fb[i];for(unsigned j=0;j<4;j++){after^=(pixel>>(8*j))&255u;after*=UINT64_C(1099511628211);}}
 if(after==hash||read_cr3()!=target||flags()&0x200||!same_cache(cache))fail("853-readback");
 arch_puts("STYLEOS S853 MOUSE");field("root",target);field("dx",event.dx);field("dy",event.dy);field("buttons",event.buttons);field("writes",moved);field("fnv",after);arch_putc('\n');
 arch_puts("STYLEOS S853 POLICY polling=yes irq=no storage=no clock=no reset=no app-admission=no production-cr3=deny\n");
#endif
#endif
 out32(0xf4,0x10);arch_halt();
}
#endif

extern bool s850_read_inherited_entry(void *,uint64_t,uint16_t,uint64_t *);
_Noreturn void s850_preflight(const struct s842_root *base,const struct s842_cache *cache,struct pg_experiment *p,const struct s846_guard *guard,uint64_t gdt,uint64_t idt,uint64_t tss,uint64_t rsp0
#ifdef STYLEOS_TEST_DESKTOP_SWITCH_851
,uint16_t gdt_limit,uint16_t tss_selector
#endif
){
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
 arch_puts("STYLEOS S850 POLICY bar-restored=yes detached-audited=yes descriptors-staged=yes desktop-root-active=no framebuffer-write=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_DESKTOP_SWITCH_851
 s851_switch(p,cache,guard,surface,gdt,idt,tss,rsp0,gdt_limit,tss_selector);
#else
 out32(0xf4,0x10);arch_halt();
#endif
}
