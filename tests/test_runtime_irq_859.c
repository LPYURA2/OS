#include <styleos/runtime_irq_859.h>
#include <assert.h>
#include <stdio.h>
int main(void){
 struct irq859_expected e={0x5f39000,IRQ859_IST,0x38,0x5da9000,0x5da9002,0x5f99580};
 struct irq859_observed o={e.root,e.ist+4096-16-40,e.cs,e.wait_lo+1,e.rsp,0x202,1,1,1};
 unsigned n=0;assert(irq859_frame_ok(&e,&o));
 #define BAD(expr) do{assert(!(expr));n++;}while(0)
 #define O(member,x) do{struct irq859_observed b=o;b.member=(x);BAD(irq859_frame_ok(&e,&b));}while(0)
 #define E(member,x) do{struct irq859_expected b=e;b.member=(x);BAD(irq859_frame_ok(&b,&o));}while(0)
 for(unsigned i=0;i<64;i++){uint64_t bit=UINT64_C(1)<<i;O(root,o.root^bit);O(frame,o.frame^bit);O(cs,o.cs^bit);O(rsp,o.rsp^bit);O(hits,o.hits^bit);O(isr,o.isr^bit);O(eoi,o.eoi^bit);E(root,e.root^bit);E(ist,e.ist^bit);E(cs,e.cs^bit);E(rsp,e.rsp^bit);}
 O(rip,e.wait_lo-1);O(rip,e.wait_hi);O(rip,0);O(rip,UINT64_MAX);E(wait_lo,e.wait_hi);E(wait_hi,e.wait_lo);E(wait_lo,0);E(wait_hi,UINT64_MAX);E(wait_hi,e.wait_lo+17);
 for(unsigned i=0;i<64;i++){uint64_t bit=UINT64_C(1)<<i;if(bit==2||bit==512||!(bit&UINT64_C(0xad7)))O(rflags,o.rflags^bit);}
 BAD(irq859_frame_ok(0,&o));BAD(irq859_frame_ok(&e,0));
 printf("IRQ frame model: 1 positive / %u refusals PASS\n",n);
}
