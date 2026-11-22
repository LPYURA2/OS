#include <styleos/pci_probe_794.h>
#include <styleos/hardware_probe_793.h>
#include <styleos/desktop_display.h>
#include <styleos/arch.h>

static void hex(uint64_t value)
{
    arch_puts("0x");
    for(int n=60;n>=0;n-=4)
        arch_putc("0123456789abcdef"[(value>>(unsigned)n)&15u]);
}
static void f(const char *name,uint64_t value)
{
    arch_putc(' ');arch_puts(name);arch_putc('=');hex(value);
}
static uint32_t in32(uint16_t port)
{
    uint32_t v;
    __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(port));
    return v;
}
static void out32(uint16_t port,uint32_t value)
{
    __asm__ volatile("outl %0,%1"::"a"(value),"Nd"(port));
}
static uint32_t cfg_read(uint8_t bus,uint8_t dev,uint8_t func,uint8_t reg)
{
    /* Only writes CF8's config ADDRESS selector; never CFC device data.
     * Single-CPU, CLI terminal probe: no concurrent PCI configuration users. */
    uint32_t addr=UINT32_C(0x80000000)|((uint32_t)bus<<16)|
                  ((uint32_t)dev<<11)|((uint32_t)func<<8)|(reg&0xfcu);
    out32(0xcf8u,addr);
    return in32(0xcfcu);
}

/* Terminal Q35 PCI bus-zero observation. Configuration mechanism #1 does not
 * enumerate bridges, validate IRQ routing or infer memory BAR aperture sizes.
 * Only an exact BAR base match is attested; BAR size is NOT measured. */
_Noreturn void pci794_terminal(const struct boot_info *info,
                              const struct desktop_surface *surface)
{
    struct hw793_map_report map;
    if(!surface || !surface->base || !surface->bytes ||
       !hw793_map_inspect(info,(uint64_t)surface->base,surface->bytes,&map)){
        arch_puts("STYLEOS S794 FAIL invalid-gop-or-map\n");arch_halt();
    }
    uint32_t prev_cf8=in32(0xcf8u);
    uint32_t devices=0,display_devices=0;
    bool matched=false;
    uint8_t match_dev=0,match_func=0;
    struct pci794_device matched_device={0};
    struct pci794_bar matched_bar={0};
    for(uint8_t dev=0;dev<32;dev++) {
        for(uint8_t fun=0;fun<8;fun++) {
            uint32_t first=cfg_read(0,dev,fun,0);
            if((first&0xffffu)==0xffffu)continue;
            uint32_t words[16];
            words[0]=first;
            for(uint8_t word=1;word<16;word++)
                words[word]=cfg_read(0,dev,fun,(uint8_t)(word*4));
            struct pci794_device parsed;
            if(!pci794_parse_header(words,&parsed))continue;
            devices++;
            if(parsed.class_code==3u &&
               (parsed.subclass==0u || parsed.subclass==0x80u)) {
                display_devices++;
                struct pci794_bar bar;
                if(pci794_find_framebuffer_bar(&parsed,(uint64_t)surface->base,&bar)) {
                    if(matched) {
                        out32(0xcf8u,prev_cf8);
                        arch_puts("STYLEOS S794 FAIL ambiguous-display-bar\n");
                        arch_halt();
                    }
                    matched=true;match_dev=dev;match_func=fun;
                    matched_bar=bar;matched_device=parsed;
                }
            }
        }
    }
    out32(0xcf8u,prev_cf8);
    arch_puts("STYLEOS S794 GOP");
    f("base",(uint64_t)surface->base);f("bytes",surface->bytes);
    f("width",surface->width);f("height",surface->height);
    f("descriptors",map.descriptor_count);
    f("uefi-overlaps",map.overlaps);arch_putc('\n');
    arch_puts("STYLEOS S794 SCAN");
    f("bus",0);f("devices",devices);f("display",display_devices);
    f("match",matched);arch_putc('\n');
    if(!matched){
        arch_puts("STYLEOS S794 GAP no-display-bar-base-match\n");
        /* No invented PCI provenance: this stage requires an exact match. */
        arch_puts("STYLEOS S794 FAIL display-bar-not-verified\n");
        arch_halt();
    }
    arch_puts("STYLEOS S794 PCI");
    f("bus",0);f("device",match_dev);f("function",match_func);
    f("vendor",matched_device.vendor);f("id",matched_device.device);
    f("class",matched_device.class_code);f("subclass",matched_device.subclass);
    f("command",matched_device.command);arch_putc('\n');
    arch_puts("STYLEOS S794 BAR");
    f("index",matched_bar.index);f("raw-lo",matched_bar.raw_lo);
    f("raw-hi",matched_bar.raw_hi);f("base",matched_bar.base);
    f("width",matched_bar.width);f("prefetch",matched_bar.prefetch);
    arch_putc('\n');
    arch_puts("STYLEOS S794 POLICY bar-base-correlated=1 bar-size=unknown bar-aperture=unverified cache-effective=unknown irq-routing=unverified firmware-runtime=unverified production-cr3=deny\n");
    arch_puts("STYLEOS S794 PASS\n");
    out32(0xf4u,0x10u);
    arch_halt();
}
