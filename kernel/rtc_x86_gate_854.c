#include <styleos/rtc_x86_854.h>
bool rtc_x86_gate_854(const struct rtc_x86_contract_854 *c, uint64_t root,
                     uint64_t rflags, uint16_t cs, bool tcg)
{
    return c && c->terminal_fixture && c->exclusive_cmos && c->known_shadow &&
        c->post_ebs && tcg && c->expected_root && !(c->expected_root & 0xfffu) &&
        root == c->expected_root && cs && !(cs & 3u) && !(rflags & 0x200u);
}
