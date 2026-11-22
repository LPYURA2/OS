#include <styleos/pci_probe_794.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void basic(uint32_t c[16])
{
    memset(c,0,16*sizeof(*c));
    c[0]=0x11111234u; /* QEMU std VGA illustrative vendor/device */
    c[1]=0x00000003u;
    c[2]=0x03000000u;
    c[3]=0x00000000u;
    c[4]=0x80000008u;
}
int main(void)
{
    uint32_t c[16];struct pci794_device d;struct pci794_bar b;
    assert(!pci794_parse_header(NULL,&d));
    assert(!pci794_parse_header(c,NULL));
    basic(c);
    assert(pci794_parse_header(c,&d));
    assert(d.vendor==0x1234u && d.device==0x1111u && d.class_code==3u);
    assert(d.bar_count==1 && d.bars[0].memory && d.bars[0].width==32);
    assert(d.bars[0].prefetch && d.bars[0].base==0x80000000u);
    assert(pci794_find_framebuffer_bar(&d,0x80000000u,&b));
    assert(!pci794_find_framebuffer_bar(&d,0x80001000u,&b));
    assert(!pci794_find_framebuffer_bar(&d,0,&b));
    assert(!pci794_find_framebuffer_bar(&d,UINT64_C(1)<<53,&b));
    c[0]=0xffffffffu; assert(!pci794_parse_header(c,&d));
    basic(c); c[4]=0x80000001u;
    assert(pci794_parse_header(c,&d));
    assert(d.bar_count==1 && !d.bars[0].memory);
    assert(!pci794_find_framebuffer_bar(&d,0x80000000u,&b));
    basic(c); c[4]=0x00000000u;
    assert(pci794_parse_header(c,&d) && d.bar_count==0);
    basic(c); c[4]=0x90000004u; c[5]=0x00000012u;
    assert(pci794_parse_header(c,&d) && d.bar_count==1);
    assert(d.bars[0].width==64 && d.bars[0].index==0 &&
           d.bars[0].base==UINT64_C(0x1290000000));
    assert(pci794_find_framebuffer_bar(&d,UINT64_C(0x1290000000),&b));
    basic(c); c[4]=0x90000002u; assert(!pci794_parse_header(c,&d));
    basic(c); c[9]=0x80000004u; assert(!pci794_parse_header(c,&d)); /* 64 BAR at 5 */
    basic(c);c[2]=0x02000000u;assert(pci794_parse_header(c,&d));
    assert(!pci794_find_framebuffer_bar(&d,0x80000000u,&b));
    basic(c);c[3]=0x00010000u;assert(!pci794_parse_header(c,&d));
    puts("STYLEOS S794 PCI PARSER HOST PASS");
    return 0;
}
