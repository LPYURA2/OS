#include <styleos/irq_apic_805.h>
#include <assert.h>
#include <stdint.h>
int main(void){
 struct irq805_sample x={0}; uint64_t base=123;
 assert(!irq805_validate(0,&base));assert(!irq805_validate(&x,0));
 x.cpuid_edx=1u<<9;x.cpuid_ecx=1u<<21;x.apic_base=UINT64_C(0xfee00900);
 x.idtr_base=UINT64_C(0x7fd000);x.idtr_limit=4095;x.cr3=UINT64_C(0x7801000);
 x.pic_master=255;x.pic_slave=255;
 assert(irq805_validate(&x,&base)&&base==UINT64_C(0xfee00000));
 x.rflags=1u<<9;assert(!irq805_validate(&x,&base));x.rflags=0;
 x.cpuid_edx=0;assert(!irq805_validate(&x,&base));x.cpuid_edx=1u<<9;
 x.apic_base=UINT64_C(0xfee00100);assert(!irq805_validate(&x,&base));
 x.apic_base=UINT64_C(0xfee00d00);x.cpuid_ecx=0;assert(!irq805_validate(&x,&base));
 x.cpuid_ecx=1u<<21;assert(irq805_validate(&x,&base));
 x.idtr_limit=10;assert(!irq805_validate(&x,&base));x.idtr_limit=4095;
 x.idtr_base=0;assert(!irq805_validate(&x,&base));x.idtr_base=UINT64_C(0x7fd000);
 x.cr3=0;assert(!irq805_validate(&x,&base));
 return 0;
}
