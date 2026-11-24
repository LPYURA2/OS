#include <styleos/rtc_decode.h>
static bool number(uint8_t raw, bool binary, unsigned *out)
{
    if (binary) { *out=raw; return true; }
    if ((raw & 15u)>9u || (raw >> 4)>9u) return false;
    *out=(raw >> 4)*10u+(raw & 15u);
    return true;
}
bool rtc_decode_854(const struct rtc_raw_854 *a, const struct rtc_raw_854 *b, struct rtc_time_854 *out)
{
    if (!a || !b || !out) return false;
    /* UIP clear, SET clear, valid RAM/time flag, stable data and data mode. */
    if ((a->status_a|b->status_a)&0x80u || (a->status_b|b->status_b)&0x80u ||
        !(a->status_d&b->status_d&0x80u) || a->status_b!=b->status_b ||
        a->sec!=b->sec || a->min!=b->min || a->hour!=b->hour) return false;
    bool binary=(a->status_b&4u)!=0, mode24=(a->status_b&2u)!=0;
    bool pm=(a->hour&0x80u)!=0;
    unsigned h,m,s;
    if ((mode24 && pm) || !number(a->hour&0x7fu,binary,&h) ||
        !number(a->min,binary,&m) || !number(a->sec,binary,&s) || m>59 || s>59) return false;
    if (mode24) { if (h>23) return false; }
    else { if (h<1 || h>12) return false; h=h%12+(pm?12:0); }
    *out=(struct rtc_time_854){(uint8_t)h,(uint8_t)m,(uint8_t)s};
    return true;
}
