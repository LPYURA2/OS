#include <styleos/rtc_owner_854.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
struct fixture { struct rtc_owner_854 *o; unsigned count, fail; uint8_t index; bool reenter; };
static bool index_write(void *ctx,uint8_t i){struct fixture*f=ctx;assert(f->o->held);if(f->reenter){struct rtc_time_854 t={255,254,253};assert(!rtc_owner_read_854(f->o,&t));assert(t.hour==255&&t.min==254&&t.sec==253);} if(++f->count==f->fail)return false;f->index=i;return true;}
static bool data_read(void *ctx,uint8_t *out){struct fixture*f=ctx;assert(f->o->held);if(++f->count==f->fail)return false;switch(f->index&127){case 0:*out=45;return true;case 2:*out=34;return true;case 4:*out=12;return true;case 10:*out=0;return true;case 11:*out=6;return true;case 13:*out=128;return true;default:assert(0);return false;}}
static const struct rtc_port_ops_854 ops={index_write,data_read};
static void unchanged(struct rtc_time_854 t){assert(t.hour==255&&t.min==254&&t.sec==253);}
int main(void){unsigned good=0,bad=0;
for(unsigned nmi=0;nmi<=128;nmi+=128){
  struct rtc_owner_854 o={0};struct fixture f={.o=&o,.index=(uint8_t)(nmi|0x22),.reenter=true};struct rtc_time_854 t={255,254,253};
  assert(!rtc_owner_read_854(&o,&t));assert(!rtc_owner_init_854(&o,&ops,&f,false,f.index));assert(!f.count);unchanged(t);bad+=2;
  assert(rtc_owner_init_854(&o,&ops,&f,true,f.index));assert(!rtc_owner_init_854(&o,&ops,&f,true,0));assert(!f.count);bad++;
  assert(rtc_owner_read_854(&o,&t));assert(t.hour==12&&t.min==34&&t.sec==45&&f.count==29&&o.shadow==(nmi|0x22)&&f.index==o.shadow&&!o.held&&!o.quarantined);good++;
  assert(rtc_owner_read_854(&o,&t));assert(f.count==58);good++;
  for(unsigned fail=1;fail<=29;fail++){
    o=(struct rtc_owner_854){0};f=(struct fixture){.o=&o,.index=(uint8_t)(nmi|0x22),.fail=fail};t=(struct rtc_time_854){255,254,253};
    assert(rtc_owner_init_854(&o,&ops,&f,true,f.index));assert(!rtc_owner_read_854(&o,&t));unchanged(t);assert(!o.held&&o.quarantined==(fail==29));bad++;
    if(fail==29){unsigned count=f.count;assert(!rtc_owner_init_854(&o,&ops,&f,true,f.index));assert(!rtc_owner_read_854(&o,&t));assert(count==f.count);unchanged(t);bad+=2;}
    else {assert(o.shadow==f.index&&f.index==(nmi|0x22));f.fail=0;assert(rtc_owner_read_854(&o,&t));assert(t.hour==12&&t.min==34&&t.sec==45);good++;}
  }
}
struct rtc_owner_854 o={0}; struct rtc_port_ops_854 partial={0};struct rtc_time_854 t={255,254,253};assert(!rtc_owner_init_854(NULL,&ops,NULL,true,0));assert(!rtc_owner_init_854(&o,NULL,NULL,true,0));assert(!rtc_owner_init_854(&o,&partial,NULL,true,0));assert(!rtc_owner_read_854(NULL,&t));assert(!rtc_owner_read_854(&o,NULL));unchanged(t);bad+=5;
printf("RTC owner: %u positive, %u refusal cases PASS; reentrant access denied, quarantine sticky\n",good,bad);}
