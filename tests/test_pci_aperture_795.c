#include <assert.h>
#include <stdio.h>
#include <styleos/pci_aperture_795.h>
int main(void){
    struct pci795_aperture a={0};
    struct pci794_bar b={.memory=true,.index=0,.width=32,
          .base=0x80000000,.raw_lo=0x80000008,.prefetch=true};
    assert(pci795_decode_aperture(&b,0xff000008,0,0x80000000,4096000,&a));
    assert(a.bytes==0x1000000 && a.end==0x81000000 && a.framebuffer_covered);
    assert(!pci795_decode_aperture(&b,0xff000008,0,0x80000000,0x1000001,&a));
    assert(!pci795_decode_aperture(&b,0xfff00008,0,0x80000000,4096000,&a));
    assert(!pci795_decode_aperture(&b,0xff000000,0,0x80000000,4096000,&a));
    assert(!pci795_decode_aperture(&b,0xfff00008,0,0x80000000,4096000,&a));
    assert(!pci795_decode_aperture(&b,0,0,0x80000000,4096000,&a));
    assert(!pci795_decode_aperture(&b,0xff000008,0,0x80001000,4096000,&a));
    b.width=64;b.raw_lo=0x8000000cu;
    assert(pci795_decode_aperture(&b,0xff00000c,0xffffffff,0x80000000,4096000,&a));
    assert(a.bytes==0x1000000);
    assert(!pci795_decode_aperture(&b,0xff00000c,0xfffffffe,0x80000000,4096000,&a));
    b.index=5;assert(!pci795_decode_aperture(&b,0xff00000c,0xffffffff,0x80000000,4096000,&a));
    puts("STYLEOS S795 BAR SIZE HOST PASS");return 0;
}
