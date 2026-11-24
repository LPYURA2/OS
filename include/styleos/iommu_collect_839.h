#ifndef STYLEOS_IOMMU_COLLECT_839_H
#define STYLEOS_IOMMU_COLLECT_839_H
#include <styleos/iommu_mapping_838.h>
/* Detached observation collection, caller-supplied validated snapshot and
 * read-only table callback. No device access or mapping changes. */
#define S839_MAX_PAGES 4u
struct s839_snapshot {
    uint64_t root,pat,cap,def;
    uint8_t physical_bits,count;
    struct cache800_pair pairs[CACHE800_MAX_VAR];
};
struct s839_result { size_t count; struct s838_page pages[S839_MAX_PAGES]; };
int s839_collect(const struct op_output *plan,struct boot_range cover,
                 const struct s839_snapshot *snapshot,cache801_read_entry reader,
                 void *context,struct s839_result *out);
#endif
