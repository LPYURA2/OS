#ifndef STYLEOS_APIC_ROOT_842_H
#define STYLEOS_APIC_ROOT_842_H
#include <styleos/paging.h>
#include <styleos/cache_policy_800.h>
#include <stdbool.h>
/* Detached QEMU-only builder. Caller must independently attest table storage,
 * live MADT/MSR addresses, same-boot MTRRs and device ownership. No activation. */
struct s842_cache {
    uint64_t pat,cap,def;
    struct cache800_pair pairs[CACHE800_MAX_VAR];
    uint8_t count,bits;
};
struct s842_root {
    struct pg_plan plan;
    struct pg_result built;
    uint64_t lapic,ioapic;
    uint8_t uc_index;
    bool ready;
};
bool s842_build(const struct pg_plan *ram, const struct boot_memory_map *memory,
                const struct s842_cache *cache,uint64_t lapic,uint64_t ioapic,
                struct s842_root *out);
bool s842_audit(const struct s842_root *root,const struct s842_cache *cache);
#endif
