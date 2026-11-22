#include <styleos/boot_info.h>
#include <styleos/pci_range_799.h>
#include <styleos/pci_topology_796.h>
#include <styleos/pci_probe_794.h>
#include <styleos/desktop_display.h>
#include <styleos/arch.h>

static void hex(uint64_t v){arch_puts("0x");for(int n=60;n>=0;n-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)n)&15u]);}
static void fld(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');hex(v);}
static uint32_t in32(uint16_t p){uint32_t x;__asm__ volatile("inl %1,%0":"=a"(x):"Nd"(p));return x;}
static void out32(uint16_t p,uint32_t x){__asm__ volatile("outl %0,%1"::"a"(x),"Nd"(p));}
static uint16_t in16(uint16_t p){uint16_t x;__asm__ volatile("inw %1,%0":"=a"(x):"Nd"(p));return x;}
static void out16(uint16_t p,uint16_t x){__asm__ volatile("outw %0,%1"::"a"(x),"Nd"(p));}
static uint32_t sel(uint8_t b,uint8_t d,uint8_t f,uint8_t off){return UINT32_C(0x80000000)|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(off&0xfcu);}
static uint32_t read32(uint8_t b,uint8_t d,uint8_t f,uint8_t off){out32(0xcf8u,sel(b,d,f,off));return in32(0xcfcu);}
static void write32(uint8_t b,uint8_t d,uint8_t f,uint8_t off,uint32_t x){out32(0xcf8u,sel(b,d,f,off));out32(0xcfcu,x);}
static uint16_t getcmd(uint8_t b,uint8_t d,uint8_t f){out32(0xcf8u,sel(b,d,f,4));return in16(0xcfcu);}
static void putcmd(uint8_t b,uint8_t d,uint8_t f,uint16_t x){out32(0xcf8u,sel(b,d,f,4));out16(0xcfcu,x);}
static _Noreturn void fail(const char *s,uint32_t cf8){out32(0xcf8u,cf8);arch_puts("STYLEOS S799 FAIL ");arch_puts(s);arch_putc('\n');arch_halt();}
struct snapshot { struct pci794_bar bar;uint32_t lo,hi,mask_lo,mask_hi;struct pci799_extent extent;};
/* Single-core, terminal QEMU-only BAR sizing; not callable from Desktop.
 * Exactly two selected memory BARs on a quiescent, unused VirtIO Serial function.
 * PCI COMMAND is restored by 16-bit write to avoid PCI STATUS W1C. */
_Noreturn void pci799_terminal(const struct boot_info *info,const struct desktop_surface *surface)
{
    (void)info;
    const uint8_t bus=1u,dev=0u,fn=0u;
    const uint8_t bridge_dev=0x1cu;
    uint32_t oldcf8=in32(0xcf8u);
    if (!surface || !surface->base || !surface->bytes)fail("invalid-display-handoff",oldcf8);
    uint32_t bw[16], ew[16];
    for(uint8_t i=0;i<16;i++){bw[i]=read32(0,bridge_dev,0,(uint8_t)(i*4));ew[i]=read32(bus,dev,fn,(uint8_t)(i*4));}
    struct pci796_bridge bridge;
    struct pci794_device ep;
    if (!pci796_parse_bridge(bw,0,&bridge) || bridge.secondary!=bus ||
        !pci794_parse_header(ew,&ep) || ep.vendor!=0x1af4u || ep.device!=0x1043u ||
        (ew[2]>>24)!=7u || !bridge.memory_window || !bridge.prefetch_window)
        fail("unexpected-bridge-or-endpoint",oldcf8);
    struct snapshot s[2]={{0}};
    bool found[2]={false,false};
    for(uint8_t i=0;i<ep.bar_count;i++){
        struct pci794_bar b=ep.bars[i];
        int n=b.index==1u?0:b.index==4u?1:-1;
        if(n<0)continue;
        if (found[n] || !b.memory || (n==0 && (b.width!=32u || b.prefetch)) ||
            (n==1 && (b.width!=64u || !b.prefetch)))fail("wrong-selected-bar",oldcf8);
        found[n]=true;s[n].bar=b;s[n].lo=b.raw_lo;s[n].hi=b.raw_hi;
    }
    if (!found[0] || !found[1])fail("missing-selected-bars",oldcf8);
    uint16_t before=getcmd(bus,dev,fn);
    if ((before&7u)==0)fail("unexpected-command-disabled",oldcf8);
    /* Stop memory, I/O and busmaster decode before writing all-one size masks. */
    putcmd(bus,dev,fn,(uint16_t)(before&~UINT16_C(7)));
    bool disabled=(getcmd(bus,dev,fn)&7u)==0u;
    if(disabled){
        for(unsigned n=0;n<2;n++){
            uint8_t off=(uint8_t)(0x10u+4u*s[n].bar.index);
            write32(bus,dev,fn,off,UINT32_MAX);
            if(s[n].bar.width==64u)write32(bus,dev,fn,(uint8_t)(off+4u),UINT32_MAX);
            s[n].mask_lo=read32(bus,dev,fn,off);
            if(s[n].bar.width==64u)s[n].mask_hi=read32(bus,dev,fn,(uint8_t)(off+4u));
            /* Restore the same BAR before probing another resource. */
            if(s[n].bar.width==64u)write32(bus,dev,fn,(uint8_t)(off+4u),s[n].hi);
            write32(bus,dev,fn,off,s[n].lo);
        }
    }
    /* Always restore both selected BARs, then PCI command. */
    for(unsigned n=0;n<2;n++){
        uint8_t off=(uint8_t)(0x10u+4u*s[n].bar.index);
        if(s[n].bar.width==64u)write32(bus,dev,fn,(uint8_t)(off+4u),s[n].hi);
        write32(bus,dev,fn,off,s[n].lo);
    }
    putcmd(bus,dev,fn,before);
    uint16_t after=getcmd(bus,dev,fn);
    bool restored=after==before;
    for(unsigned n=0;n<2;n++){
        uint8_t off=(uint8_t)(0x10u+4u*s[n].bar.index);
        restored=restored && read32(bus,dev,fn,off)==s[n].lo &&
          (s[n].bar.width!=64u || read32(bus,dev,fn,(uint8_t)(off+4u))==s[n].hi);
    }
    out32(0xcf8u,oldcf8);
    bool valid=disabled && restored;
    for(unsigned n=0;n<2;n++){
        valid=valid && pci799_decode(&s[n].bar,s[n].mask_lo,s[n].mask_hi,&s[n].extent);
        if(valid){
            valid=pci799_forwarded(&s[n].extent,n==0?bridge.memory_base:bridge.prefetch_base,
                                   n==0?bridge.memory_end:bridge.prefetch_end);
        }
    }
    if(s[0].extent.base < s[1].extent.end && s[1].extent.base < s[0].extent.end)valid=false;
    arch_puts("STYLEOS S799 PCI");fld("bus",bus);fld("device",dev);fld("function",fn);
    fld("vendor",ep.vendor);fld("id",ep.device);fld("bridge-bus",bridge.primary);
    fld("bridge-device",bridge_dev);fld("secondary",bridge.secondary);arch_putc('\n');
    arch_puts("STYLEOS S799 STATE");fld("command-before",before);fld("command-after",after);
    fld("decode-disabled",disabled);fld("registers-restored",restored);arch_putc('\n');
    for(unsigned n=0;n<2;n++){
        arch_puts("STYLEOS S799 BAR");fld("index",s[n].bar.index);
        fld("width",s[n].bar.width);fld("prefetch",s[n].bar.prefetch);
        fld("raw-lo",s[n].lo);fld("raw-hi",s[n].hi);
        fld("mask-lo",s[n].mask_lo);fld("mask-hi",s[n].mask_hi);
        fld("base",s[n].bar.base);fld("size",s[n].extent.size);fld("end",s[n].extent.end);
        fld("window-base",n==0?bridge.memory_base:bridge.prefetch_base);
        fld("window-end",n==0?bridge.memory_end:bridge.prefetch_end);arch_putc('\n');
    }
    arch_puts("STYLEOS S799 POLICY range-proven=");arch_puts(valid?"two-downstream-bars":"no");
    arch_puts(" cache-effective=unknown all-bars=unmeasured irq-routing=unknown firmware-runtime=unknown production-cr3=deny\n");
    if(!valid)fail("sizing-restoration-or-bridge",oldcf8);
    arch_puts("STYLEOS S799 PASS\n");out32(0xf4u,0x10u);arch_halt();
}
