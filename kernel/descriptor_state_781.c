#include <styleos/descriptor_state_781.h>

static uint64_t u64(const uint8_t *p)
{
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8u*i);
    return v;
}

bool dsi781_current_rsp_in_boot_stack(uint64_t rsp,
                                      uint64_t boot_start,
                                      uint64_t boot_end)
{
    return boot_start && boot_start < boot_end && boot_end < UINT64_C(1)<<47 &&
           rsp >= boot_start && rsp < boot_end;
}

bool dsi781_audit_idt(const uint8_t *idt, size_t extent,
                      uint16_t cs, uint16_t tr, uint16_t gdt_limit,
                      struct dsi781_report *out)
{
    if (out) *out = (struct dsi781_report){0};
    if (!out || !idt || extent != 4096u || !cs || (cs & 7u) ||
        cs > gdt_limit || (uint32_t)cs+7u > gdt_limit ||
        gdt_limit >= 4096u || (tr && ((tr & 7u) ||
        (uint32_t)tr + 15u > gdt_limit ||
        ((uint32_t)tr <= (uint32_t)cs + 7u &&
         (uint32_t)cs <= (uint32_t)tr + 15u)))) return false;
    struct dsi781_report r = {
        .cs = cs, .tr = tr, .tss_descriptor_unresolved = true,
        .tss_stack_unresolved = true, .activation_permitted = false
    };
    for (unsigned i = 0; i < 256u; ++i) {
        const uint8_t *p = idt + 16u*i;
        uint64_t low = u64(p), high = u64(p+8u);
        uint64_t target = (low & UINT64_C(0xffff)) |
                          ((low >> 48u) << 16u) | (high << 32u);
        const unsigned ist = (unsigned)((low >> 32u) & 0xffu);
        const unsigned attr = (unsigned)((low >> 40u) & 0xffu);
        if (((low >> 16u) & 0xffffu) != cs || (ist & ~7u) ||
            attr != 0x8eu || high >> 32u || !target ||
            target >= (UINT64_C(1) << 47u)) return false;
        if (ist) {
            ++r.ist_gates;
            r.ist_index_mask |= (uint8_t)(1u << (ist-1u));
        }
        ++r.gates;
    }
    /* A live IST gate without a task register is an inconsistent machine
     * state, not evidence of an IST pointer. Do not promote a stale TR. */
    if (!tr && r.ist_gates) return false;
    *out = r;
    return true;
}
