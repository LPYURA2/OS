/* Diagnostic-only COM2 gate after S816 PASS, immediately before debug exit.
 * Include only under a new dedicated QEMU profile macro. No IOMMU MMIO. */
#include <stdint.h>
#include <styleos/arch.h>
static inline void s834_out(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%%dx"::"a"(v),"d"(p):"memory");}
static inline uint8_t s834_in(uint16_t p){uint8_t v;__asm__ volatile("inb %%dx,%0":"=a"(v):"d"(p):"memory");return v;}
static inline int s834_gate(void){
    const uint16_t p=0x2f8;
    s834_out(p+1,0);s834_out(p+3,0x80);s834_out(p,1);s834_out(p+1,0);
    s834_out(p+3,3);s834_out(p+2,7);s834_out(p+4,3);
    if(s834_in(p+5)&1u)return 0; /* stale input is not a release */
    arch_puts("STYLEOS S834 GATE post-ebs=yes wait-com2=yes\n");
    for(uint32_t i=0;i<UINT32_MAX;i++){
        uint8_t s=s834_in(p+5);if(s&0x1eu)return 0;
        if(s&1u){if(s834_in(p)!=0xa5u)return 0;arch_puts("STYLEOS S834 GATE release=yes\n");return 1;}
        __asm__ volatile("pause":::"memory");
    }
    return 0;
}
