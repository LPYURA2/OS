#ifndef STYLEOS_RTC_CMOS_854_H
#define STYLEOS_RTC_CMOS_854_H
#include <styleos/rtc_acquire.h>
/* The owner must serialize all CMOS users (including NMI handlers), supply
 * the known index/NMI shadow and keep ownership until release. No index-port
 * readback is assumed. On restoration failure, release must quarantine CMOS. */
struct rtc_cmos_bus_854 {
    bool (*claim)(void *ctx, uint8_t *saved_index);
    bool (*write_index)(void *ctx, uint8_t index);
    bool (*read_data)(void *ctx, uint8_t *data);
    void (*release)(void *ctx, bool restored);
};
bool rtc_cmos_read_854(const struct rtc_cmos_bus_854 *bus, void *ctx,
                       struct rtc_time_854 *out);
#endif
