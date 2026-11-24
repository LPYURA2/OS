#include <styleos/calibration_855.h>
/* Reject broad measurements individually: overlapping uncertainty alone
 * cannot turn three poor measurements into a credible calibration. */
static bool narrow(uint64_t lo,uint64_t hi)
{
    return lo && hi>=lo && hi<=S855_MAX_HZ && hi-lo<=lo/100;
}
static bool interval(const struct calibration_window_855 *w,uint64_t *lo,uint64_t *hi)
{
    if(w->start_before>w->start_after || w->start_after>=w->end_before ||
       w->end_before>w->end_after || !w->ref_ticks_min ||
       w->ref_ticks_min>w->ref_ticks_max || w->ref_ticks_max>65536u) return false;
    uint64_t min=w->end_before-w->start_after, max=w->end_after-w->start_before;
    if(max>UINT64_MAX/S855_PIT_NOMINAL_HZ)return false;
    uint64_t a=min*S855_PIT_NOMINAL_HZ,b=max*S855_PIT_NOMINAL_HZ;
    *lo=a/w->ref_ticks_max;
    *hi=b/w->ref_ticks_min+(b%w->ref_ticks_min!=0);
    return narrow(*lo,*hi);
}
bool calibration_pit_855(const struct calibration_window_855 *w,unsigned count,
                         struct calibration_result_855 *out)
{
    if(!w||!out||count!=S855_CAL_SAMPLES)return false;
    uint64_t lo=0,hi=S855_MAX_HZ;
    for(unsigned i=0;i<count;i++){
        uint64_t a,b;
        if((i&&w[i].start_before<w[i-1].end_after)||!interval(&w[i],&a,&b))return false;
        if(a>lo)lo=a;
        if(b<hi)hi=b;
        if(hi<lo)return false;
    }
    if(!narrow(lo,hi))return false;
    *out=(struct calibration_result_855){lo,hi,lo+(hi-lo)/2};return true;
}
