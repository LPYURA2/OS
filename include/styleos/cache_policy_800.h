#ifndef STYLEOS_CACHE_POLICY_800_H
#define STYLEOS_CACHE_POLICY_800_H
#include <stdint.h>
#include <stdbool.h>
#define CACHE800_MAX_VAR 16u
#define CACHE800_UNRESOLVED 255u
struct cache800_pair {uint64_t base,mask;};
struct cache800_result {uint8_t type;uint8_t matched;bool mtrr_enabled;};
/* Only read-only MTRR PHYSICAL-domain observation, NOT effective PAT/PTE
 * cache selection. No MMIO mapping or production CR3 permission follows. */
bool cache800_physical_type(uint64_t addr,uint8_t phys_bits,
                           uint64_t cap,uint64_t def,
                           const struct cache800_pair *pairs,
                           uint8_t count,struct cache800_result *out);
#endif
