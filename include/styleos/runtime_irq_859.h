#ifndef STYLEOS_RUNTIME_IRQ_859_H
#define STYLEOS_RUNTIME_IRQ_859_H
#include <stdint.h>
#include <stdbool.h>
#define IRQ859_VECTOR 0x50u
#define IRQ859_IST UINT64_C(0x400000003000)
struct irq859_expected {uint64_t root,ist,cs,wait_lo,wait_hi,rsp;};
struct irq859_observed {uint64_t root,frame,cs,rip,rsp,rflags,hits,isr,eoi;};
/* Pure check of trusted kernel observations; not a hardware authorization. */
static inline bool irq859_frame_ok(const struct irq859_expected *e,const struct irq859_observed *o){
 return e&&o&&e->root&&!(e->root&4095u)&&e->root<(UINT64_C(1)<<47)&&e->ist==IRQ859_IST&&e->cs&&e->cs<=0xffff&&!(e->cs&7u)&&e->wait_lo&&e->wait_lo<e->wait_hi&&e->wait_hi-e->wait_lo<=16&&e->wait_hi<(UINT64_C(1)<<47)&&e->rsp&&!(e->rsp&7u)&&e->rsp<(UINT64_C(1)<<47)&&o->root==e->root&&o->frame==e->ist+4096-16-40&&o->cs==e->cs&&o->rip>=e->wait_lo&&o->rip<e->wait_hi&&o->rsp==e->rsp&&(o->rflags&0x202)==0x202&&!(o->rflags&~UINT64_C(0xad7))&&o->hits==1&&o->isr==1&&o->eoi==1;
}
/* Terminal single-CPU test; no scheduling or reusable IRQ grant. */
void irq859_once(uint64_t root,uint64_t idt,uint16_t cs,uint64_t stack_lo,uint64_t stack_hi);
#endif
