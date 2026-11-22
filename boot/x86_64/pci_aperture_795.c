#include <styleos/pci_aperture_795.h>
#include <styleos/hardware_probe_793.h>
#include <styleos/desktop_display.h>
#include <styleos/arch.h>

static void hex(uint64_t v){arch_puts("0x");for(int n=60;n>=0;n-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)n)&15u]);}
static void field(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');hex(v);}
static uint32_t in32(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static void out32(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static uint16_t in16(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static void out16(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static uint32_t sel(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t reg){return 0x80000000u | ((uint32_t)bus<<16) | ((uint32_t)dev<<11) | ((uint32_t)fn<<8) | (reg&0xfcu);}
static uint32_t read32(uint8_t b,uint8_t d,uint8_t f,uint8_t r){out32(0xcf8u,sel(b,d,f,r));return in32(0xcfcu);}
static void write32(uint8_t b,uint8_t d,uint8_t f,uint8_t r,uint32_t v){out32(0xcf8u,sel(b,d,f,r));out32(0xcfcu,v);}
static uint16_t command(uint8_t b,uint8_t d,uint8_t f){out32(0xcf8u,sel(b,d,f,4));return in16(0xcfcu);}
static void set_command(uint8_t b,uint8_t d,uint8_t f,uint16_t v){out32(0xcf8u,sel(b,d,f,4));out16(0xcfcu,v);}
static _Noreturn void fail(const char *msg,uint32_t restore_cf8){out32(0xcf8u,restore_cf8);arch_puts("STYLEOS S795 FAIL ");arch_puts(msg);arch_putc('\n');arch_halt();}

/* This diagnostic is TERMINAL and virtual-machine-only. It runs after
 * ExitBootServices and CLI, in a single-vCPU QEMU boot. Unlike 794, PCI BAR
 * register values are TEMPORARILY modified using the standard size probe.
 * Memory decoding and bus mastering are first disabled; original BAR and
 * command are always restored before policy evaluation or exit. Never run
 * this function from Desktop or while a driver owns the device. */
_Noreturn void pci795_terminal(const struct boot_info *info,
                              const struct desktop_surface *surface)
{
    uint32_t old_cf8=in32(0xcf8u);
    struct hw793_map_report map;
    if(!surface || !surface->base || !surface->bytes ||
       !hw793_map_inspect(info,(uint64_t)surface->base,surface->bytes,&map))
        fail("invalid-gop",old_cf8);
    bool match=false;uint8_t match_bus=0,match_dev=0,match_fn=0;
    struct pci794_bar bar={0};struct pci794_device device={0};
    uint32_t device_count=0;
    /* Scope is bus 0 for 795: other buses and bridges are NOT attested. */
    for(uint8_t d=0;d<32;d++)for(uint8_t f=0;f<8;f++){
        uint32_t first=read32(0,d,f,0);
        if((first&0xffffu)==0xffffu)continue;
        uint32_t w[16];w[0]=first;
        for(uint8_t i=1;i<16;i++)w[i]=read32(0,d,f,(uint8_t)(4u*i));
        struct pci794_device parsed;
        if(!pci794_parse_header(w,&parsed))continue;
        device_count++;
        struct pci794_bar candidate;
        if(pci794_find_framebuffer_bar(&parsed,(uint64_t)surface->base,&candidate)){
            if(match)fail("ambiguous-display-bar",old_cf8);
            match=true;match_dev=d;match_fn=f;bar=candidate;device=parsed;
        }
    }
    if(!match || bar.index>5u || !bar.memory ||
       (bar.width==64u && bar.index>4u))fail("no-unique-bar",old_cf8);
    uint8_t offset=(uint8_t)(0x10u + 4u*bar.index);
    uint16_t before_cmd=command(match_bus,match_dev,match_fn);
    uint32_t before_low=read32(match_bus,match_dev,match_fn,offset);
    uint32_t before_high=bar.width==64u?read32(match_bus,match_dev,match_fn,(uint8_t)(offset+4u)):0;
    if(before_low!=bar.raw_lo || before_high!=bar.raw_hi)
        fail("bar-mutated-before-test",old_cf8);
    /* Do not touch PCI status (W1C): command is exactly one 16-bit write. */
    set_command(match_bus,match_dev,match_fn,(uint16_t)(before_cmd & ~UINT16_C(0x0007)));
    bool disabled=(command(match_bus,match_dev,match_fn)&UINT16_C(0x0007))==0;
    uint32_t mask_low=0,mask_high=0;
    if(disabled){
        write32(match_bus,match_dev,match_fn,offset,UINT32_MAX);
        if(bar.width==64u)write32(match_bus,match_dev,match_fn,(uint8_t)(offset+4u),UINT32_MAX);
        mask_low=read32(match_bus,match_dev,match_fn,offset);
        if(bar.width==64u)mask_high=read32(match_bus,match_dev,match_fn,(uint8_t)(offset+4u));
    }
    /* Restoration is unconditional, even for a malformed hardware response. */
    if(bar.width==64u)write32(match_bus,match_dev,match_fn,(uint8_t)(offset+4u),before_high);
    write32(match_bus,match_dev,match_fn,offset,before_low);
    set_command(match_bus,match_dev,match_fn,before_cmd);
    uint16_t after_cmd=command(match_bus,match_dev,match_fn);
    bool restored=read32(match_bus,match_dev,match_fn,offset)==before_low &&
        (bar.width!=64u || read32(match_bus,match_dev,match_fn,(uint8_t)(offset+4u))==before_high) &&
        after_cmd==before_cmd;
    out32(0xcf8u,old_cf8);
    struct pci795_aperture aperture;
    bool valid=disabled && restored && pci795_decode_aperture(&bar,mask_low,mask_high,
                 (uint64_t)surface->base,surface->bytes,&aperture);
    arch_puts("STYLEOS S795 GOP");field("base",(uint64_t)surface->base);
    field("bytes",surface->bytes);field("width",surface->width);field("height",surface->height);
    field("uefi-overlaps",map.overlaps);arch_putc('\n');
    arch_puts("STYLEOS S795 PCI");field("bus",match_bus);field("device",match_dev);field("function",match_fn);
    field("vendor",device.vendor);field("id",device.device);field("devices",device_count);
    field("index",bar.index);field("width",bar.width);field("base",bar.base);arch_putc('\n');
    arch_puts("STYLEOS S795 SIZING");field("cmd-before",before_cmd);
    field("cmd-restored",after_cmd);
    field("mask-lo",mask_low);field("mask-hi",mask_high);
    field("decode-disabled",disabled);field("registers-restored",restored);
    field("size",valid?aperture.bytes:0);field("end",valid?aperture.end:0);arch_putc('\n');
    arch_puts("STYLEOS S795 POLICY range=");arch_puts(valid?"verified":"unverified");
    arch_puts(" cache-effective=unknown all-pci=unverified irq-routing=unverified firmware-runtime=unverified production-cr3=deny\n");
    if(!valid)fail("sizing-or-restore",old_cf8);
    arch_puts("STYLEOS S795 PASS\n");out32(0xf4u,0x10u);arch_halt();
}
