#include <styleos/pci_topology_796.h>
#include <styleos/pci_probe_794.h>
#include <styleos/hardware_probe_793.h>
#include <styleos/desktop_display.h>
#include <styleos/arch.h>

static void hex(uint64_t value){arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(value>>(unsigned)i)&15u]);}
static void f(const char *name,uint64_t v){arch_putc(' ');arch_puts(name);arch_putc('=');hex(v);}
static uint32_t in32(uint16_t port){uint32_t x;__asm__ volatile("inl %1,%0":"=a"(x):"Nd"(port));return x;}
static void out32(uint16_t port,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(port));}
static uint32_t readcfg(uint8_t b,uint8_t d,uint8_t fn,uint8_t off){
    out32(0xcf8u,UINT32_C(0x80000000)|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)fn<<8)|(off&0xfcu));
    return in32(0xcfcu);
}
static _Noreturn void fail(const char *reason,uint32_t cf8){
    out32(0xcf8u,cf8);arch_puts("STYLEOS S796 FAIL ");arch_puts(reason);arch_putc('\n');arch_halt();
}
/* Read-only configuration-space scan. There is no CFC write, no BAR sizing,
 * no access to MMIO registers, no IRQ activation, and no PCI driver binding. */
_Noreturn void pci796_terminal(const struct boot_info *info,
                            const struct desktop_surface *surface)
{
    uint32_t saved_cf8=in32(0xcf8u);
    struct hw793_map_report map;
    if(!surface || !surface->base || !surface->bytes ||
       !hw793_map_inspect(info,(uint64_t)surface->base,surface->bytes,&map))
        fail("invalid-gop",saved_cf8);
    uint8_t queue[256]={0};bool visited[256]={0};
    uint16_t head=0,tail=1;queue[0]=0;visited[0]=true;
    uint32_t devices=0,bridges=0,bar_mem=0,bar_io=0,bar_unknown=0;
    uint32_t secondary_routes=0,display_matches=0,bridge_mem_windows=0;
    uint32_t all_function_headers=0;uint8_t disp_bus=0,disp_dev=0,disp_fn=0;
    bool gop_found=false;
    while(head<tail){
        uint8_t bus=queue[head++];
        for(uint8_t dev=0;dev<32;dev++)for(uint8_t fn=0;fn<8;fn++){
            uint32_t first=readcfg(bus,dev,fn,0);
            if((first&0xffffu)==0xffffu || (first&0xffffu)==0)continue;
            uint32_t w[16];w[0]=first;
            for(uint8_t i=1;i<16;i++)w[i]=readcfg(bus,dev,fn,(uint8_t)(4u*i));
            uint8_t type=(uint8_t)((w[3]>>16)&0x7fu);
            if(type!=0 && type!=1)fail("unsupported-header",saved_cf8);
            if(++all_function_headers>4096u)fail("device-overflow",saved_cf8);
            devices++;
            arch_puts("STYLEOS S796 DEVICE");
            f("bus",bus);f("device",dev);f("function",fn);
            f("vendor",(uint16_t)w[0]);f("id",(uint16_t)(w[0]>>16));
            f("class",w[2]>>24);f("subclass",(w[2]>>16)&0xffu);f("header",type);
            arch_putc('\n');
            if(type==1){
                struct pci796_bridge bridge;
                if(!pci796_parse_bridge(w,bus,&bridge))fail("invalid-bridge-route",saved_cf8);
                if(!pci796_queue_secondary(bridge.secondary,queue,&tail,visited))
                    fail("duplicate-or-overflow-secondary",saved_cf8);
                bridges++;secondary_routes++;
                if(bridge.memory_window)bridge_mem_windows++;
                arch_puts("STYLEOS S796 BRIDGE");f("primary",bridge.primary);
                f("secondary",bridge.secondary);f("subordinate",bridge.subordinate);
                f("memory-base",bridge.memory_window?bridge.memory_base:0);
                f("memory-end",bridge.memory_window?bridge.memory_end:0);
                f("prefetch-base",bridge.prefetch_window?bridge.prefetch_base:0);
                f("prefetch-end",bridge.prefetch_window?bridge.prefetch_end:0);
                arch_putc('\n');
                continue;
            }
            struct pci794_device ep;
            if(!pci794_parse_header(w,&ep))fail("bad-endpoint-bars",saved_cf8);
            for(uint8_t i=0;i<ep.bar_count;i++){
                const struct pci794_bar *bar=&ep.bars[i];
                if(bar->memory)bar_mem++;else bar_io++;
                bar_unknown++;
                arch_puts("STYLEOS S796 BAR");f("bus",bus);f("device",dev);f("function",fn);
                f("index",bar->index);f("memory",bar->memory);
                f("base",bar->base);f("width",bar->width);f("prefetch",bar->prefetch);
                arch_putc('\n');
                if(bar->memory && bar->base==(uint64_t)surface->base &&
                   ep.class_code==3u && (ep.subclass==0u || ep.subclass==0x80u)){
                    display_matches++;disp_bus=bus;disp_dev=dev;disp_fn=fn;
                    gop_found=true; /* correlated base only; no size claim */
                }
            }
        }
    }
    out32(0xcf8u,saved_cf8);
    arch_puts("STYLEOS S796 INVENTORY");
    f("buses",head);f("devices",devices);f("bridges",bridges);
    f("routes",secondary_routes);f("bridge-windows",bridge_mem_windows);
    f("memory-bars",bar_mem);f("io-bars",bar_io);f("bar-sizes-unknown",bar_unknown);
    f("display-matches",display_matches);arch_putc('\n');
    arch_puts("STYLEOS S796 GOP");f("base",(uint64_t)surface->base);
    f("bytes",surface->bytes);f("bus",disp_bus);f("device",disp_dev);f("function",disp_fn);
    f("observed-nonzero",gop_found);arch_putc('\n');
    arch_puts("STYLEOS S796 POLICY pci-configuration=read-only bar-sizes=unknown bridge-window-sizes=observed-not-owned cache-effective=unknown irq-routing=unverified firmware-runtime=unverified production-cr3=deny\n");
    if(display_matches!=1 || devices==0 || bar_mem==0)
        fail("incomplete-or-ambiguous-display",saved_cf8);
    arch_puts("STYLEOS S796 PASS\n");out32(0xf4u,0x10u);arch_halt();
}
