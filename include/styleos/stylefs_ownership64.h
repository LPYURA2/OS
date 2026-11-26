#ifndef STYLEOS_OWNERSHIP64_H
#define STYLEOS_OWNERSHIP64_H
#include <styleos/stylefs_inodecommit64.h>
#include <styleos/stylefs_alloc64.h>
/* Planning only, trusted serialized supervisor with immutable snapshots.
 * Inventory storage supplied by caller, O(number of allocated extents),
 * never proportional to disk bytes. Each retained bank is checked, including
 * the inactive bank. No writes, reservation, repair or automatic reclaim.
 * Any nonblank invalid receipt or table refuses planning conservatively. */
enum ow64_status {OW64_OK,OW64_INVALID,OW64_STALE,OW64_CORRUPT,OW64_IO,OW64_OUTPUT,OW64_FULL};
struct ow64_extent {struct sfs64_run run;uint64_t id,revision,bytes,logical;uint32_t checksum;};
struct ow64_inventory {
 struct ow64_extent *entries;
 struct sfs64_run *runs;
 size_t slots,count;
};
struct ow64_result {uint64_t epoch,capacity,data_start;struct sfs64_space space;bool valid;};
/* Both inventories are scratch until success. Cross-snapshot overlapping
 * runs must be exactly identical mappings of the same immutable revision.
 * A successful inventory can be used by existing allocator ONLY while the
 * caller holds the same exclusive grants and prevents all mutation. */
enum ow64_status ow64_collect(struct ic64_state *,struct ic64_workspace *,
 struct ow64_inventory banks[2],struct ow64_result *);
#endif
