#include <styleos/rtc_x86_854.h>
/* No raw I/O operation may run before the terminal fixture's owner gate.
 * x86 IN/OUT instructions provide no hardware success/restore acknowledgement. */
static uint64_t flags(void){uint64_t v;__asm__ volatile("pushfq;popq %0":"=r"(v)::"memory");return v;}
static uint64_t root(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v)::"memory");return v;}
static uint16_t cs_read(void){uint16_t v;__asm__ volatile("mov %%cs,%0":"=r"(v));return v;}
static bool tcg_fixture(void)
{
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(1),"2"(0));
    if(!(c & (1u<<31))) return false;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(0x40000000u),"2"(0));
    return b==0x54474354u && c==0x43544743u && d==0x47435447u;
}
static bool live(const struct rtc_x86_854 *x)
{
    return x && x->expected_root && !(cs_read() & 3u) &&
        root()==x->expected_root && !(flags() & 0x200u);
}
static bool index_write(void *ctx, uint8_t value)
{
    struct rtc_x86_854 *x=ctx;
    if(!live(x) || !x->owner.held || x->owner.quarantined) return false;
    __asm__ volatile("outb %0,$0x70"::"a"(value):"memory");
    return true;
}
static bool data_read(void *ctx, uint8_t *value)
{
    struct rtc_x86_854 *x=ctx;uint8_t v;
    if(!value || !live(x) || !x->owner.held || x->owner.quarantined) return false;
    __asm__ volatile("inb $0x71,%0":"=a"(v)::"memory");
    *value=v;return true;
}
bool rtc_x86_bind_854(struct rtc_x86_854 *x,const struct rtc_x86_contract_854 *c)
{
    static const struct rtc_port_ops_854 ops={index_write,data_read};
    if(!x || !c || x->expected_root || x->owner.admitted || x->owner.held ||
        x->owner.quarantined || (cs_read()&3u)) return false;
    if(!rtc_x86_gate_854(c,root(),flags(),cs_read(),tcg_fixture())) return false;
    if(!rtc_owner_init_854(&x->owner,&ops,x,true,c->known_index)) return false;
    x->expected_root=c->expected_root;
    return true;
}
bool rtc_x86_read_854(struct rtc_x86_854 *x,struct rtc_time_854 *out)
{
    if(!out || !live(x)) return false;
    struct rtc_time_854 t;
    if(!rtc_owner_read_854(&x->owner,&t)) return false;
    if(!live(x)){x->owner.quarantined=true;return false;}
    *out=t;return true;
}
