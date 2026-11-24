#ifndef STYLEOS_RTC_OWNER_854_H
#define STYLEOS_RTC_OWNER_854_H
#include <styleos/rtc_cmos_854.h>
/* Single-CPU, externally serialized, exclusive CMOS ownership. Initial index
 * comes from an explicit platform handoff, never an assumed port-70 read. */
struct rtc_port_ops_854 {
    bool (*index)(void *, uint8_t);
    bool (*data)(void *, uint8_t *);
};
struct rtc_owner_854 {
    const struct rtc_port_ops_854 *ops;
    void *ctx;
    uint8_t shadow, saved;
    bool admitted, held, quarantined;
};
bool rtc_owner_init_854(struct rtc_owner_854 *, const struct rtc_port_ops_854 *, void *, bool known_exclusive, uint8_t initial);
bool rtc_owner_read_854(struct rtc_owner_854 *, struct rtc_time_854 *);
#endif
