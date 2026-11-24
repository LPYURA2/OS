#ifndef STYLEOS_RTC_ACQUIRE_854_H
#define STYLEOS_RTC_ACQUIRE_854_H
#include <styleos/rtc_decode.h>
/* Backend must own the CMOS index/NMI policy and serialize access.
 * No status-C reads, no RTC configuration writes. */
typedef bool (*rtc_read_854)(void *ctx, uint8_t reg, uint8_t *value);
bool rtc_acquire_854(rtc_read_854 read, void *ctx, struct rtc_time_854 *out);
#endif
