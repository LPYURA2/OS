#include <styleos/rtc_acquire.h>
static bool snapshot(rtc_read_854 read, void *ctx, struct rtc_raw_854 *r)
{
    uint8_t after;
    if (!read(ctx, 0x0a, &r->status_a) || (r->status_a & 0x80)) return false;
    if (!read(ctx, 0x0b, &r->status_b) || !read(ctx, 0x0d, &r->status_d) ||
        !read(ctx, 0, &r->sec) || !read(ctx, 2, &r->min) ||
        !read(ctx, 4, &r->hour) || !read(ctx, 0x0a, &after) ||
        (after & 0x80)) return false;
    return true;
}
bool rtc_acquire_854(rtc_read_854 read, void *ctx, struct rtc_time_854 *out)
{
    if (!read || !out) return false;
    for (unsigned i=0; i<8; ++i) {
        struct rtc_raw_854 a,b;
        if (snapshot(read,ctx,&a) && snapshot(read,ctx,&b) && rtc_decode_854(&a,&b,out)) return true;
    }
    return false;
}
