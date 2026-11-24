#include <styleos/calibration_855.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t seed=0x123456789abcdefULL;
static uint64_t rng(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static void fixture(struct calibration_window_855 *w,uint64_t hz){
    uint64_t start=0;
    for(unsigned i=0;i<3;i++){
        uint32_t ticks=50000+(uint32_t)(rng()%10000);
        uint64_t delta=(uint64_t)(((__uint128_t)hz*ticks)/S855_PIT_NOMINAL_HZ);
        w[i]=(struct calibration_window_855){start,start+10,start+delta-10,start+delta+10,ticks-1,ticks+1};
        start=w[i].end_after+100;
    }
}
static void reject(const struct calibration_window_855 *w,unsigned count){struct calibration_result_855 r={11,22,33};assert(!calibration_pit_855(w,count,&r));assert(r.min_hz==11&&r.max_hz==22&&r.estimate_hz==33);}
int main(void){
    for(unsigned i=0;i<100000;i++){
        uint64_t hz=1000000+rng()%9900000000ULL;
        struct calibration_window_855 w[3];fixture(w,hz);
        struct calibration_result_855 r;
        assert(calibration_pit_855(w,3,&r));assert(r.min_hz<=hz&&hz<=r.max_hz);
        uint64_t lo=0,hi=S855_MAX_HZ;
        for(unsigned j=0;j<3;j++){
            __uint128_t a=(__uint128_t)(w[j].end_before-w[j].start_after)*S855_PIT_NOMINAL_HZ;
            __uint128_t b=(__uint128_t)(w[j].end_after-w[j].start_before)*S855_PIT_NOMINAL_HZ;
            uint64_t l=a/w[j].ref_ticks_max,h=(b+w[j].ref_ticks_min-1)/w[j].ref_ticks_min;
            if(l>lo)lo=l;
            if(h<hi)hi=h;
        }
        assert(r.min_hz==lo&&r.max_hz==hi&&r.estimate_hz==lo+(hi-lo)/2);
        struct monotonic_855 m={0};assert(monotonic_init_855(&m,r.estimate_hz,0));assert(monotonic_sample_855(&m,r.estimate_hz)&&m.ns==S855_NS_PER_SECOND);
    }
    struct calibration_window_855 base[3],w[3];fixture(base,2000000000);
    reject(NULL,3);reject(base,0);reject(base,2);reject(base,4);assert(!calibration_pit_855(base,3,NULL));
    for(unsigned i=0;i<3;i++)for(unsigned k=0;k<10;k++){
        memcpy(w,base,sizeof w);struct calibration_window_855 *p=w+i;
        switch(k){case 0:p->start_before=p->start_after+1;break;case 1:p->end_before=p->start_after;break;case 2:p->end_after=p->end_before-1;break;case 3:p->ref_ticks_min=0;break;case 4:p->ref_ticks_max=p->ref_ticks_min-1;break;case 5:p->ref_ticks_max=65537;break;case 6:p->end_after=UINT64_MAX;break;case 7:p->ref_ticks_min=1;break;case 8:p->ref_ticks_min/=2;p->ref_ticks_max/=2;break;case 9:p->start_after=p->end_before-1;break;}
        reject(w,3);
    }
    memcpy(w,base,sizeof w);w[1].start_before=w[0].end_after-1;reject(w,3);
    puts("Calibration: 100000 independent-oracle and monotonic integration cases PASS; 36 refusal cases PASS");
}
