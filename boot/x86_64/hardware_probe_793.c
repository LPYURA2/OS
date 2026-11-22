#include <styleos/hardware_probe_793.h>
#include <styleos/desktop_display.h>
#include <styleos/arch.h>

static void hex(uint64_t number)
{
    arch_puts("0x");
    for (int shift=60;shift>=0;shift-=4)
        arch_putc("0123456789abcdef"[(number>>(unsigned)shift)&15u]);
}
static void field(const char *name,uint64_t number)
{
    arch_putc(' ');arch_puts(name);arch_putc('=');hex(number);
}
static uint8_t in8(uint16_t port)
{
    uint8_t v;
    __asm__ volatile ("inb %1,%0":"=a"(v):"Nd"(port));
    return v;
}
static uint64_t msr(uint32_t index)
{
    uint32_t low,high;
    __asm__ volatile ("rdmsr":"=a"(low),"=d"(high):"c"(index));
    return ((uint64_t)high<<32)|low;
}
static uint32_t cpuid1_edx(void)
{
    uint32_t a,b,c,d;
    __asm__ volatile ("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    (void)a;(void)b;(void)c;
    return d;
}

/* Terminal diagnostic ONLY, after ExitBootServices. All UEFI services are dead.
 * We only read firmware's copied descriptor buffer, hardware CPU registers and
 * legacy PIC masks; no writes to framebuffer, paging or device controllers. */
_Noreturn void hw793_terminal(const struct boot_info *info,
                             const struct desktop_surface *surface)
{
    struct hw793_map_report report;
    if (!surface || !surface->base || !surface->bytes ||
        !hw793_map_inspect(info,(uint64_t)surface->base,surface->bytes,&report)){
        arch_puts("STYLEOS S793 FAIL invalid-gop-or-map\n");arch_halt();
    }
    uint64_t flags,cr0,cr3;
    __asm__ volatile("pushfq; popq %0":"=r"(flags));
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    uint32_t caps=cpuid1_edx();
    if (!(caps&(1u<<5))) {arch_puts("STYLEOS S793 FAIL no-msr\n");arch_halt();}
    uint64_t pat=msr(0x277u);
    uint64_t mtrr=(caps&(1u<<12))?msr(0x2ffu):0;
    arch_puts("STYLEOS S793 GOP");
    field("base",surface->base);field("bytes",surface->bytes);
    field("width",surface->width);field("height",surface->height);
    field("stride",surface->stride);field("pixel",surface->format);
    field("page-start",report.page_start);field("page-end",report.page_end);
    arch_putc('\n');
    arch_puts("STYLEOS S793 MAP");
    field("descriptors",report.descriptor_count);
    field("overlaps",report.overlaps);
    field("covered",report.covered_bytes);
    field("complete",report.all_pages_described);
    field("mixed",report.type_mixed);
    field("type",report.matched_type);
    field("attributes",report.attribute_or);
    field("runtime",report.runtime_count);
    arch_putc('\n');
    arch_puts("STYLEOS S793 CPU");
    field("cr0",cr0);field("cr3",cr3);
    field("rflags",flags);field("cpuid-edx",caps);
    field("pat",pat);field("mtrr-default",mtrr);
    field("mtrr-supported",!!(caps&(1u<<12)));
    arch_putc('\n');
    arch_puts("STYLEOS S793 PORT");
    field("com1",0x3f8);field("ps2-data",0x60);
    field("ps2-cmd",0x64);field("ata-primary",0x1f0);
    field("pic-master-mask",in8(0x21));field("pic-slave-mask",in8(0xa1));
    field("if",!!(flags&(1u<<9)));
    arch_putc('\n');
    arch_puts("STYLEOS S793 POLICY cache-effective=unknown mmio-bars=unverified irq-routing=unverified firmware-runtime=unverified production-cr3=deny\n");
    if(!report.overlaps){arch_puts("STYLEOS S793 FAIL no-framebuffer-descriptor\n");arch_halt();}
    arch_puts("STYLEOS S793 PASS\n");
    __asm__ volatile("outl %0,%1"::"a"(0x10u),"Nd"((uint16_t)0xf4u));
    arch_halt();
}
