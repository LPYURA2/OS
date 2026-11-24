#ifndef STYLEOS_IOMMU_MAPPING_838_H
#define STYLEOS_IOMMU_MAPPING_838_H
#include <styleos/ownership.h>
#include <styleos/cache_pagewalk_801.h>
#include <styleos/cache_policy_800.h>
/* Pure observation contract. Input must be from a separately proven same-boot
 * profile. No target access, permission change or ownership transfer. */
struct s838_page {
    uint64_t address;
    struct cache801_translation first,last;
    struct cache800_result physical_cache;
};
int s838_observe(const struct op_output *plan, struct boot_range cover,
                 const struct s838_page *pages, size_t count);
#endif
