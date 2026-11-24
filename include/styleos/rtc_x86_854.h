#ifndef STYLEOS_RTC_X86_854_H
#define STYLEOS_RTC_X86_854_H
#include <styleos/rtc_owner_854.h>
/* Terminal QEMU-only adapter. Caller must prove exclusive CMOS access and
 * establish the known index/NMI shadow after EBS. It must not infer the
 * shadow by reading port 0x70. No runtime/production admission. */
struct rtc_x86_contract_854 {
    uint64_t expected_root;
    uint8_t known_index;
    bool terminal_fixture, exclusive_cmos, known_shadow, post_ebs;
};
struct rtc_x86_854 {
    struct rtc_owner_854 owner;
    uint64_t expected_root;
};
bool rtc_x86_gate_854(const struct rtc_x86_contract_854 *, uint64_t root,
                      uint64_t rflags, uint16_t cs, bool tcg);
bool rtc_x86_bind_854(struct rtc_x86_854 *, const struct rtc_x86_contract_854 *);
bool rtc_x86_read_854(struct rtc_x86_854 *, struct rtc_time_854 *);
#endif
