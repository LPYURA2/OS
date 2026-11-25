#ifndef STYLEOS_RTC_RUNTIME_863_H
#define STYLEOS_RTC_RUNTIME_863_H
#include <styleos/rtc_owner_854.h>
/* Runtime-only admission boundary. The platform must supply real same-boot
 * observations, exclusive CMOS ownership and a known software index shadow.
 * This module grants no ports and does not establish those facts itself. */
struct rtc_runtime_observation_863 {
    uint64_t root, epoch, rflags;
    uint16_t cs;
    bool q35_tcg, single_cpu, post_ebs, exclusive_cmos, known_shadow;
};
struct rtc_runtime_ops_863 {
    bool (*observe)(void *, struct rtc_runtime_observation_863 *);
    bool (*index)(void *, uint8_t);
    bool (*data)(void *, uint8_t *);
};
struct rtc_runtime_863 {
    struct rtc_owner_854 owner;
    const struct rtc_runtime_ops_863 *ops;
    void *ctx;
    uint64_t root, epoch;
    bool busy, bound, quarantined;
};
bool rtc_runtime_bind_863(struct rtc_runtime_863 *,
    const struct rtc_runtime_ops_863 *, void *, uint64_t root,
    uint64_t epoch, uint8_t known_index);
bool rtc_runtime_read_863(struct rtc_runtime_863 *, struct rtc_time_854 *);
#endif
