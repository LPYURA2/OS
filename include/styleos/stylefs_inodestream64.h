#ifndef STYLEOS_INODESTREAM64_H
#define STYLEOS_INODESTREAM64_H
#include <styleos/stylefs_ownership64.h>
/* Append-only inode creation, not namespace publication. Supervisor owns
 * two exclusive disjoint grants on same device, serializes all operations.
 * IDs strictly increase beyond both retained tables. No raw concurrent I/O.
 * Existing allocator/window/manifest/commit functions are reused. */
enum { IS64_RUNS=(IT64_PAGE-IT64_HEADER-4-SFS64_MAP_HEADER)/SFS64_MAP_RECORD };
enum is64_status {IS64_OK,IS64_INVALID,IS64_STALE,IS64_FULL,IS64_CANCELLED,IS64_IO,IS64_UNCERTAIN,IS64_CORRUPT};
struct is64_workspace {
 struct ic64_workspace commit;
 uint8_t old_page[IT64_PAGE],manifest[IT64_PAGE],buffer[SFS64_WINDOW_BYTES];
 struct it64_blob old[IT64_MAX_RECORDS];
 struct sfs64_run plan[IS64_RUNS];
};
typedef bool (*is64_source)(void *,uint64_t,uint8_t *,size_t);
enum is64_status is64_create(struct ic64_state *,uint64_t data_token,struct is64_workspace *,
 struct ow64_inventory banks[2],uint64_t id,uint64_t bytes,is64_source,void *);
/* Copy-on-write replacement of an exact current revision. Old extents
 * remain protected by both-bank allocation. Same ID and new revision. */
enum is64_status is64_replace(struct ic64_state *,uint64_t,struct is64_workspace *,
 struct ow64_inventory banks[2],uint64_t id,uint64_t revision,uint64_t bytes,is64_source,void *);
#endif
