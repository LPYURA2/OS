#ifndef STYLEOS_RTC_DECODE_854_H
#define STYLEOS_RTC_DECODE_854_H
#include <stdbool.h>
#include <stdint.h>
/* Pure candidate model. No hardware access, no production integration. */
struct rtc_raw_854 { uint8_t sec, min, hour, status_a, status_b, status_d; };
struct rtc_time_854 { uint8_t hour, min, sec; };
bool rtc_decode_854(const struct rtc_raw_854 *a, const struct rtc_raw_854 *b, struct rtc_time_854 *out);
#endif
