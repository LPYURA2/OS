#include <styleos/monotonic_855.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
static uint64_t rng=UINT64_C(0x8e9a4d7d0147b0c3);
static uint64_t random64(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
static __uint128_t oracle(uint64_t delta,uint64_t hz){return (__uint128_t)delta*S855_NS_PER_SECOND/hz;}
static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
int main(void){
    unsigned randomized=0, sequences=0, refusals=0, boundaries=0;
    for(unsigned i=0;i<200000;i++){
        uint64_t hz=(random64()%S855_MAX_HZ)+1;
        uint64_t origin=random64(),span=UINT64_MAX-origin;
        uint64_t delta=random64();if(span!=UINT64_MAX)delta%=span+1;
        struct monotonic_855 s={0};CHECK(monotonic_init_855(&s,hz,origin));
        __uint128_t expected=oracle(delta,hz);
        bool ok=monotonic_sample_855(&s,origin+delta);
        CHECK(ok==(expected<=UINT64_MAX));
        if(ok){CHECK(s.ns==(uint64_t)expected);CHECK(s.last_counter==origin+delta&&!s.faulted);}
        else{CHECK(s.ns==0&&s.last_counter==origin&&s.faulted);CHECK(!monotonic_sample_855(&s,origin));}
        randomized++;
    }
    const uint64_t rates[]={1,3,10,1193182,1000000,24000000,1000000000,3000000000ULL,9999999999ULL,S855_MAX_HZ};
    for(size_t i=0;i<sizeof rates/sizeof *rates;i++){
        struct monotonic_855 s={0};CHECK(monotonic_init_855(&s,rates[i],42));
        for(uint64_t d=0;d<10000;d++){CHECK(monotonic_sample_855(&s,42+d));CHECK(s.ns==oracle(d,rates[i]));sequences++;}
        uint64_t last=s.ns;CHECK(monotonic_sample_855(&s,s.last_counter));CHECK(s.ns==last);
        uint64_t counter=s.last_counter;CHECK(!monotonic_sample_855(&s,counter-1));CHECK(s.faulted&&s.ns==last&&s.last_counter==counter);CHECK(!monotonic_sample_855(&s,counter+1));CHECK(!monotonic_init_855(&s,rates[i],0));refusals+=3;
    }
    /* Maximum delta accepted by floor(delta * 1e9 / hz) <= UINT64_MAX.
     * Oracle calculation deliberately uses wider host arithmetic. */
    for(size_t i=0;i<sizeof rates/sizeof *rates;i++){
        __uint128_t cap=(((__uint128_t)UINT64_MAX+1)*rates[i]-1)/S855_NS_PER_SECOND;
        uint64_t max=cap>UINT64_MAX?UINT64_MAX:(uint64_t)cap;
        struct monotonic_855 b={0};CHECK(monotonic_init_855(&b,rates[i],0));
        CHECK(monotonic_sample_855(&b,max));CHECK(b.ns==oracle(max,rates[i]));boundaries++;
        if(max!=UINT64_MAX){uint64_t before=b.ns;CHECK(!monotonic_sample_855(&b,max+1));CHECK(b.faulted&&b.ns==before&&b.last_counter==max);boundaries++;}
    }
    struct monotonic_855 s={0};CHECK(!monotonic_sample_855(&s,0));CHECK(!monotonic_init_855(NULL,1,0));CHECK(!monotonic_sample_855(NULL,0));CHECK(!monotonic_init_855(&s,0,0));CHECK(!monotonic_init_855(&s,S855_MAX_HZ+1,0));refusals+=5;
    CHECK(monotonic_init_855(&s,1,0));CHECK(!monotonic_init_855(&s,1,1));refusals++;
    uint64_t limit=UINT64_MAX/S855_NS_PER_SECOND;CHECK(monotonic_sample_855(&s,limit));CHECK(s.ns==limit*S855_NS_PER_SECOND);CHECK(!monotonic_sample_855(&s,limit+1));CHECK(s.ns==limit*S855_NS_PER_SECOND&&s.faulted);refusals++;
    s=(struct monotonic_855){0};CHECK(monotonic_init_855(&s,S855_NS_PER_SECOND,0));CHECK(monotonic_sample_855(&s,UINT64_MAX));CHECK(s.ns==UINT64_MAX);CHECK(!monotonic_sample_855(&s,0));CHECK(s.ns==UINT64_MAX&&s.faulted);refusals++;
    s=(struct monotonic_855){0};CHECK(monotonic_init_855(&s,1000000000,UINT64_MAX-1));CHECK(monotonic_sample_855(&s,UINT64_MAX));CHECK(s.ns==1);CHECK(!monotonic_sample_855(&s,0));CHECK(s.ns==1&&s.faulted);refusals++;
    for(unsigned mode=0;mode<4;mode++){s=(struct monotonic_855){0};CHECK(monotonic_init_855(&s,3,100));CHECK(monotonic_sample_855(&s,103));switch(mode){case 0:s.hz=0;break;case 1:s.hz=S855_MAX_HZ+1;break;case 2:s.origin=104;break;case 3:s.ns++;break;}uint64_t before=s.ns;CHECK(!monotonic_sample_855(&s,104));CHECK(s.faulted&&s.ns==before&&s.last_counter==103);refusals++;}
    printf("PASS: %u randomized oracle comparisons, %u sequential samples, %u exact boundary cases, %u explicit refusal cases, %u assertions\n",randomized,sequences,boundaries,refusals,checks);
}
