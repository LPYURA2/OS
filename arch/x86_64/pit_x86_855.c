#include <styleos/pit_x86_855.h>
static uint64_t root(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v)::"memory");return v;}
static uint64_t flags(void){uint64_t v;__asm__ volatile("pushfq;popq %0":"=r"(v)::"memory");return v;}
static uint16_t cs_read(void){uint16_t v;__asm__ volatile("mov %%cs,%0":"=r"(v));return v;}
static uint8_t in8(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p):"memory");return v;}
static void out8(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p):"memory");}
static bool live(const struct pit_x86_855 *x){return x&&x->bound&&!(cs_read()&3u)&&!(flags()&0x200u)&&!x->capture.quarantined&&x->root==root();}
static bool owned(const struct pit_x86_855 *x){return live(x)&&x->capture.busy;}
bool pit_x86_bind_855(struct pit_x86_855 *x,const struct pit_x86_contract_855 *c)
{
    if(!x||!c||x->bound||x->root||x->capture.busy||x->capture.quarantined||(cs_read()&3u))return false;
    uint32_t a,b,d,e;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(e),"=d"(d):"0"(1),"2"(0));
    bool hv=!!(e&(1u<<31)),tsc=!!(d&(1u<<4)),sse2=!!(d&(1u<<26));
    if(!hv)return false;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(e),"=d"(d):"0"(0x40000000u),"2"(0));
    bool tcg=b==0x54474354u&&e==0x43544743u&&d==0x47435447u;
    if(!pit_x86_gate_855(c,root(),flags(),cs_read(),tcg,tsc,sse2))return false;
    x->root=c->root;x->bound=true;return true;
}
bool pit_x86_counter_855(struct pit_x86_855 *x,uint64_t *out)
{
    if(!out||!live(x))return false;
    uint32_t lo,hi;__asm__ volatile("lfence;rdtsc;lfence":"=a"(lo),"=d"(hi)::"memory");
    if(!live(x))return false;
    *out=((uint64_t)hi<<32)|lo;return true;
}
static bool stop(void *ctx){struct pit_x86_855 *x=ctx;if(!owned(x))return false;out8(0x61,0);return !(in8(0x61)&3u);}
static bool prepare(void *ctx,uint16_t count){struct pit_x86_855 *x=ctx;if(!count||!stop(x))return false;out8(0x43,0xb0);out8(0x42,(uint8_t)count);out8(0x42,(uint8_t)(count>>8));return !(in8(0x61)&3u);}
static bool gate_on(void *ctx){struct pit_x86_855 *x=ctx;if(!owned(x))return false;out8(0x61,1);return (in8(0x61)&3u)==1;}
static bool output(void *ctx,bool *out){struct pit_x86_855 *x=ctx;if(!out||!owned(x))return false;*out=!!(in8(0x61)&0x20u);return true;}
static bool counter(void *ctx,uint64_t *out){return owned(ctx)&&pit_x86_counter_855(ctx,out);}
bool pit_x86_window_855(struct pit_x86_855 *x,const struct pit_capture_contract_855 *c,struct calibration_window_855 *out)
{
    static const struct pit_capture_ops_855 ops={prepare,counter,gate_on,output,stop};
    if(!live(x))return false;
    return pit_capture_window_855(&x->capture,&ops,x,c,out);
}
