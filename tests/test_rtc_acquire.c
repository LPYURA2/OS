#include <styleos/rtc_acquire.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
struct fixture {unsigned calls, fault, kind;};
static bool read_reg(void *ctx, uint8_t reg, uint8_t *value)
{
    struct fixture *f=ctx;
    unsigned n=++f->calls;
    assert(n<=112);
    if(f->kind==1 && n==f->fault) return false;
    if(f->kind==2) return false;
    switch(reg){
    case 0x0a: *value=(f->kind==3 || (f->kind==4 && n%7==0) || (f->kind==5 && n==1))?0x80:0;return true;
    case 0x0b: *value=6;return true;
    case 0x0d: *value=f->kind==6?0:0x80;return true;
    case 0: *value=f->kind==7?(n/7)%60:45;return true;
    case 2: *value=34;return true;
    case 4: *value=12;return true;
    default: assert(0);return false;
    }
}
static void good(unsigned kind,unsigned fault){struct fixture f={0,fault,kind};struct rtc_time_854 o={255,254,253};assert(rtc_acquire_854(read_reg,&f,&o));assert(o.hour==12&&o.min==34&&o.sec==45);assert(f.calls<=112);}
static void bad(unsigned kind){struct fixture f={0,0,kind};struct rtc_time_854 o={255,254,253};assert(!rtc_acquire_854(read_reg,&f,&o));assert(o.hour==255&&o.min==254&&o.sec==253);assert(f.calls<=112);}
int main(void){good(0,0);for(unsigned n=1;n<=14;n++)good(1,n);good(5,0);bad(2);bad(3);bad(4);bad(6);bad(7);struct rtc_time_854 o={255,254,253};assert(!rtc_acquire_854(NULL,NULL,&o));assert(!rtc_acquire_854(read_reg,NULL,NULL));puts("RTC acquisition: 16 success/retry, 7 refusal, <=112 reads per call, output preserved on failure");}
