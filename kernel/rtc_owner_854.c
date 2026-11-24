#include <styleos/rtc_owner_854.h>
bool rtc_owner_init_854(struct rtc_owner_854 *o, const struct rtc_port_ops_854 *ops, void *ctx, bool known, uint8_t initial)
{
    /* One-shot initialization. A zero-initialized object is required. Never
     * silently clear quarantine, or replace a live ownership contract. */
    if (!o || o->admitted || o->held || o->quarantined || !known || !ops || !ops->index || !ops->data) return false;
    *o=(struct rtc_owner_854){.ops=ops,.ctx=ctx,.shadow=initial,.admitted=true};
    return true;
}
static bool claim(void *ctx,uint8_t *saved)
{
    struct rtc_owner_854 *o=ctx;
    if (!o->admitted || o->held || o->quarantined) return false;
    o->held=true; o->saved=o->shadow; *saved=o->saved; return true;
}
static bool index_write(void *ctx,uint8_t value)
{
    struct rtc_owner_854 *o=ctx;
    if (!o->held || o->quarantined || ((value^o->saved)&128)) return false;
    if (value!=o->saved) switch(value&127) {
        case 0: case 2: case 4: case 10: case 11: case 13: break;
        default:return false;
    }
    if (!o->ops->index(o->ctx,value)) return false;
    o->shadow=value; return true;
}
static bool data_read(void *ctx,uint8_t *out)
{
    struct rtc_owner_854 *o=ctx;
    if (!o->held || o->quarantined) return false;
    return o->ops->data(o->ctx,out);
}
static void release(void *ctx,bool restored)
{
    struct rtc_owner_854 *o=ctx;
    if (!restored || o->shadow!=o->saved) o->quarantined=true;
    o->held=false;
}
bool rtc_owner_read_854(struct rtc_owner_854 *o,struct rtc_time_854 *out)
{
    static const struct rtc_cmos_bus_854 bus={claim,index_write,data_read,release};
    if (!o || !out || !o->admitted || o->held || o->quarantined) return false;
    return rtc_cmos_read_854(&bus,o,out);
}
