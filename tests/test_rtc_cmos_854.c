#include <styleos/rtc_cmos_854.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
struct fixture { unsigned ops,fail,claims,releases; uint8_t initial,index; bool owned,blocked,quarantined,unstable; };
static bool claim(void *ctx,uint8_t *saved){struct fixture *f=ctx;f->claims++;if(f->blocked||f->quarantined||f->owned)return false;f->owned=true;*saved=f->initial;return true;}
static bool write_index(void *ctx,uint8_t index){struct fixture *f=ctx;assert(f->owned);assert((index&128)==(f->initial&128));uint8_t r=index&127;assert(index==f->initial||r==0||r==2||r==4||r==10||r==11||r==13);if(++f->ops==f->fail)return false;f->index=index;return true;}
static bool read_data(void *ctx,uint8_t *out){struct fixture *f=ctx;assert(f->owned);if(++f->ops==f->fail)return false;switch(f->index&127){case 0:*out=45;return true;case 2:*out=34;return true;case 4:*out=12;return true;case 10:*out=f->unstable?128:0;return true;case 11:*out=6;return true;case 13:*out=128;return true;default:assert(0);return false;}}
static void release(void *ctx,bool restored){struct fixture *f=ctx;assert(f->owned);f->owned=false;f->releases++;f->quarantined=!restored;if(restored)assert(f->index==f->initial);}
static const struct rtc_cmos_bus_854 bus={claim,write_index,read_data,release};
static struct fixture fixture(uint8_t initial){return (struct fixture){.initial=initial,.index=initial};}
static void unchanged(struct rtc_time_854 t){assert(t.hour==255&&t.min==254&&t.sec==253);}
int main(void){unsigned good=0,bad=0;for(unsigned nmi=0;nmi<=128;nmi+=128){
    struct fixture f=fixture((uint8_t)(nmi|0x22));struct rtc_time_854 t={255,254,253};
    assert(rtc_cmos_read_854(&bus,&f,&t));assert(t.hour==12&&t.min==34&&t.sec==45&&f.ops==29&&f.claims==1&&f.releases==1&&!f.owned&&!f.quarantined);good++;
    for(unsigned fault=1;fault<=29;fault++){f=fixture((uint8_t)(nmi|0x22));f.fail=fault;t=(struct rtc_time_854){255,254,253};assert(!rtc_cmos_read_854(&bus,&f,&t));unchanged(t);assert(f.claims==1&&f.releases==1&&!f.owned);assert(f.quarantined==(fault==29));assert(f.ops<=29);bad++;}
    f=fixture((uint8_t)(nmi|0x22));f.blocked=true;t=(struct rtc_time_854){255,254,253};assert(!rtc_cmos_read_854(&bus,&f,&t));unchanged(t);assert(!f.ops&&!f.releases);bad++;
    f=fixture((uint8_t)(nmi|0x22));f.unstable=true;assert(!rtc_cmos_read_854(&bus,&f,&t));unchanged(t);assert(f.ops==17&&f.releases==1&&!f.quarantined);bad++;
    f=fixture((uint8_t)(nmi|0x22));f.fail=29;assert(!rtc_cmos_read_854(&bus,&f,&t));assert(!rtc_cmos_read_854(&bus,&f,&t));unchanged(t);assert(f.ops==29&&f.releases==1&&f.quarantined);bad++;
    }
    struct fixture f=fixture(0x22);struct rtc_time_854 t={255,254,253};struct rtc_cmos_bus_854 partial=bus;partial.release=NULL;assert(!rtc_cmos_read_854(&partial,&f,&t));assert(!rtc_cmos_read_854(NULL,&f,&t));assert(!rtc_cmos_read_854(&bus,&f,NULL));assert(!f.claims&&!f.ops);unchanged(t);bad+=3;
    printf("CMOS adapter: %u positive, %u refusal scenarios PASS; index/NMI preserved, no CMOS data writes, quarantine on restore failure\n",good,bad);
}
