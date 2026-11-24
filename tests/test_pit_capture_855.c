#include <styleos/pit_capture_855.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {struct pit_capture_855 *state; unsigned calls,fail,reads,high_at,stops; uint64_t t; bool stop_fail,initial_high,backwards,reenter;};
static const struct pit_capture_contract_855 contract={59659,59657,59661,5,true,true,true};
static const struct pit_capture_ops_855 ops;
static bool step(struct fixture *f){assert(f->state->busy);return ++f->calls!=f->fail;}
static bool prepare(void *p,uint16_t n){struct fixture*f=p;assert(n==59659);if(f->reenter){struct calibration_window_855 out={0};assert(!pit_capture_window_855(f->state,&ops,f,&contract,&out));}return step(f);}
static bool counter(void*p,uint64_t*v){struct fixture*f=p;if(!step(f))return false;f->t+=10;*v=f->backwards&&f->calls==5?0:f->t;return true;}
static bool gate(void*p){return step(p);}
static bool output(void*p,bool*v){struct fixture*f=p;if(!step(f))return false;*v=f->reads++==0?f->initial_high:f->reads>=f->high_at;return true;}
static bool stop(void*p){struct fixture*f=p;f->stops++;return step(f)&&!f->stop_fail;}
static const struct pit_capture_ops_855 ops={prepare,counter,gate,output,stop};
static struct calibration_window_855 sentinel(void){return (struct calibration_window_855){1,2,3,4,5,6};}
static void unchanged(struct calibration_window_855 w){assert(w.start_before==1&&w.start_after==2&&w.end_before==3&&w.end_after==4&&w.ref_ticks_min==5&&w.ref_ticks_max==6);}
int main(void){unsigned positive=0,negative=0;
for(unsigned h=3;h<=6;h++){struct pit_capture_855 s={0};struct fixture f={.state=&s,.high_at=h,.reenter=true};struct calibration_window_855 w=sentinel();assert(pit_capture_window_855(&s,&ops,&f,&contract,&w));assert(w.start_before==10&&w.start_after==20&&w.end_before==30+20*(h-3)&&w.end_after==60+20*(h-3)&&w.ref_ticks_min==59657&&w.ref_ticks_max==59661);assert(f.stops==1&&!s.busy&&!s.quarantined);positive++;}
/* A successful h=4 run contains 15 operations, including cleanup. */
struct pit_capture_855 s={0};struct fixture f={.state=&s,.high_at=4};struct calibration_window_855 w=sentinel();assert(pit_capture_window_855(&s,&ops,&f,&contract,&w));unsigned total=f.calls;positive++;
for(unsigned fail=1;fail<=total;fail++){s=(struct pit_capture_855){0};f=(struct fixture){.state=&s,.high_at=4,.fail=fail};w=sentinel();assert(!pit_capture_window_855(&s,&ops,&f,&contract,&w));unchanged(w);assert(f.stops==1&&!s.busy&&s.quarantined==(fail==total));negative++;}
for(unsigned kind=0;kind<5;kind++){s=(struct pit_capture_855){0};f=(struct fixture){.state=&s,.high_at=4};w=sentinel();switch(kind){case 0:f.initial_high=true;break;case 1:f.high_at=2;break;case 2:f.high_at=100;break;case 3:f.backwards=true;break;case 4:f.stop_fail=true;break;}assert(!pit_capture_window_855(&s,&ops,&f,&contract,&w));unchanged(w);assert(f.stops==1&&!s.busy&&s.quarantined==(kind==4));if(kind==4){unsigned n=f.calls;assert(!pit_capture_window_855(&s,&ops,&f,&contract,&w)&&f.calls==n);negative++;}negative++;}
for(unsigned k=0;k<10;k++){s=(struct pit_capture_855){0};f=(struct fixture){.state=&s,.high_at=4};w=sentinel();struct pit_capture_contract_855 c=contract;switch(k){case 0:c.terminal=false;break;case 1:c.exclusive=false;break;case 2:c.reference_attested=false;break;case 3:c.count=0;break;case 4:c.ref_ticks_min=0;break;case 5:c.ref_ticks_min=60000;break;case 6:c.ref_ticks_max=1;break;case 7:c.ref_ticks_max=65537;break;case 8:c.max_polls=0;break;case 9:c.max_polls=S855_CAPTURE_MAX_POLLS+1;break;}assert(!pit_capture_window_855(&s,&ops,&f,&c,&w)&&!f.calls);unchanged(w);negative++;}
printf("PIT capture model: %u positive / %u refusal PASS; %u individual operation faults; no hardware I/O\n",positive,negative,total);}
