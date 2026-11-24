#include <styleos/rtc_decode.h>
#include <assert.h>
#include <stdio.h>
static uint8_t encode(unsigned n, bool bin){return bin?n:((n/10)<<4)|(n%10);}
int main(void){
    unsigned valid=0, rejected=0;
    for(unsigned mode=0;mode<4;mode++)for(unsigned h=0;h<24;h++)for(unsigned m=0;m<60;m++)for(unsigned s=0;s<60;s++){
        bool bin=mode&1, h24=mode&2;
        unsigned hr=h24?h:(h%12?h%12:12);
        struct rtc_raw_854 a={encode(s,bin),encode(m,bin),encode(hr,bin)|(!h24&&h>=12?128:0),0,(bin?4:0)|(h24?2:0),128};
        struct rtc_time_854 o={255,255,255};
        assert(rtc_decode_854(&a,&a,&o)&&o.hour==h&&o.min==m&&o.sec==s);valid++;
    }
    struct rtc_raw_854 a={0x59,0x59,0x23,0,2,128};
    for(unsigned i=0;i<12;i++){
        struct rtc_raw_854 b=a,c=a;
        switch(i){case 0:b.status_a=128;break;case 1:c.status_a=128;break;case 2:b.status_b|=128;break;case 3:c.status_d=0;break;case 4:c.sec=0;break;case 5:b.min=c.min=0x1a;break;case 6:b.sec=c.sec=0x60;break;case 7:b.hour=c.hour=0x24;break;case 8:b.hour=c.hour=0xa3;break;case 9:b.status_b=c.status_b=0;b.hour=c.hour=0;break;case 10:b.status_b=c.status_b=0;b.hour=c.hour=0x13;break;case 11:c.status_b=6;break;}
        struct rtc_time_854 out={251,252,253};assert(!rtc_decode_854(&b,&c,&out)&&out.hour==251&&out.min==252&&out.sec==253);rejected++;
    }
    struct rtc_time_854 out={251,252,253};assert(!rtc_decode_854(0,&a,&out));assert(!rtc_decode_854(&a,0,&out));assert(!rtc_decode_854(&a,&a,0));rejected+=3;
    printf("RTC model: %u valid, %u rejected, no output mutation on refusal\n",valid,rejected);
}
