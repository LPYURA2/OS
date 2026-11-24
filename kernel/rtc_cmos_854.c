#include <styleos/rtc_cmos_854.h>
struct access { const struct rtc_cmos_bus_854 *bus; void *ctx; uint8_t saved; bool failed; };
static bool read_reg(void *ctx, uint8_t reg, uint8_t *value)
{
    struct access *a=ctx;
    if (a->failed) return false;
    switch(reg) {
        case 0: case 2: case 4: case 0x0a: case 0x0b: case 0x0d: break;
        default: a->failed=true; return false;
    }
    /* Preserve the owner-supplied NMI bit. Never write CMOS data. */
    if (!a->bus->write_index(a->ctx,(a->saved&0x80u)|reg) ||
        !a->bus->read_data(a->ctx,value)) { a->failed=true; return false; }
    return true;
}
bool rtc_cmos_read_854(const struct rtc_cmos_bus_854 *bus, void *ctx,
                       struct rtc_time_854 *out)
{
    if (!bus || !out || !bus->claim || !bus->write_index ||
        !bus->read_data || !bus->release) return false;
    struct access a={.bus=bus,.ctx=ctx};
    if (!bus->claim(ctx,&a.saved)) return false;
    struct rtc_time_854 candidate;
    bool valid=rtc_acquire_854(read_reg,&a,&candidate);
    bool restored=bus->write_index(ctx,a.saved);
    bus->release(ctx,restored);
    if (!valid || a.failed || !restored) return false;
    *out=candidate;
    return true;
}
