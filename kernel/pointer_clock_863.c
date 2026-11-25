#include <styleos/pointer_clock_863.h>
bool pc863_init(struct pc863_clock *s,const struct pc863_edge e[3])
{
    if(!s||!e||s->mono.ready||s->mono.faulted||s->min_hz||s->max_hz)return false;
    uint64_t lo=0,hi=S855_MAX_HZ;
    for(unsigned i=0;i<3;i++){
        if(!e[i].lo||e[i].hi<e[i].lo||e[i].second>=86400)return false;
        if(!i)continue;
        if(e[i].second!=(e[i-1].second+1)%86400||e[i].lo<=e[i-1].hi)return false;
        uint64_t a=e[i].lo-e[i-1].hi,b=e[i].hi-e[i-1].lo;
        if(!a||b>S855_MAX_HZ||b-a>a/100)return false;
        if(a>lo)lo=a;
        if(b<hi)hi=b;
    }
    if(!lo||hi<lo||hi-lo>lo/100)return false;
    struct monotonic_855 m={0};
    if(!monotonic_init_855(&m,lo+(hi-lo)/2,e[2].hi))return false;
    s->mono=m;s->min_hz=lo;s->max_hz=hi;return true;
}
uint64_t pc863_sample(struct pc863_clock *s,uint64_t counter)
{
    if(!s||!monotonic_sample_855(&s->mono,counter))return 0;
    if(s->mono.ns==UINT64_MAX){s->mono.faulted=true;return 0;}
    return s->mono.ns+1; /* zero is reserved for unavailable */
}
bool pc863_near(const struct pc863_clock *s,uint64_t a,uint64_t b)
{
    return s&&s->mono.ready&&!s->mono.faulted&&s->mono.ns<UINT64_MAX&&
        a&&b>a&&b<=s->mono.ns+1&&b-a<=UINT64_C(450000000);
}
