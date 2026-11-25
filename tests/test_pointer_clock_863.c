#include <styleos/pointer_clock_863.h>
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const struct pc863_edge e[3]={{1000,1002,86399},{1000001000,1000001002,0},{2000001000,2000001002,1}};
    struct pc863_clock s={0};
    assert(pc863_init(&s,e)&&s.min_hz==999999998&&s.max_hz==1000000002&&s.mono.hz==1000000000);
    uint64_t a=pc863_sample(&s,e[2].hi);
    assert(a==1&&!pc863_near(&s,a,a)&&!pc863_init(&s,e));
    uint64_t b=pc863_sample(&s,e[2].hi+450000000);
    assert(b==450000001&&pc863_near(&s,a,b)&&!pc863_near(&s,0,b)&&!pc863_near(&s,a,b+1));
    b=pc863_sample(&s,e[2].hi+450000001);
    assert(b==450000002&&!pc863_near(&s,a,b));
    assert(!pc863_sample(&s,e[2].hi)&&s.mono.faulted&&!pc863_near(&s,a,b));
    for(unsigned i=0;i<6;i++){
        struct pc863_edge bad[3]={e[0],e[1],e[2]};s=(struct pc863_clock){0};
        switch(i){
        case 0:bad[0].lo=0;break;
        case 1:bad[1].second=2;break;
        case 2:bad[1].hi=bad[1].lo-1;break;
        case 3:bad[1].lo=bad[0].hi;break;
        case 4:bad[1].hi+=20000000;break;
        case 5:bad[2].lo+=100000000;bad[2].hi+=100000000;break;
        }
        assert(!pc863_init(&s,bad)&&!s.mono.ready&&!s.min_hz&&!s.max_hz&&!pc863_sample(&s,1));
    }
    puts("S863 pointer clock RTC bracket and monotonic threshold PASS (host only)");
}
