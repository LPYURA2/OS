#ifndef STYLEOS_INODECOMMIT64_H
#define STYLEOS_INODECOMMIT64_H
#include <styleos/stylefs_inodetable64.h>
/* Two disjoint banks in dedicated metadata grant: two receipt sectors
 * followed by equally sized banks of 4KiB pages. Immutable snapshot.
 * Trusted serialized caller owns all buffers and the exclusive grant.
 * Data extents must be independently proven durable and disjoint.
 * This is not yet a replacement of the existing mounted file journal. */
enum ic64_status {IC64_OK,IC64_INVALID,IC64_CORRUPT,IC64_IO,IC64_FULL,IC64_STALE,IC64_CANCELLED,IC64_UNCERTAIN};
struct ic64_state {
 struct it64_state table;
 uint64_t sectors;
 unsigned active;
};
struct ic64_workspace {
 uint8_t page[IT64_PAGE],packed[IT64_PAGE],next[IT64_PAGE];
 struct it64_blob blobs[IT64_MAX_RECORDS];
};
/* Source supplies one encoded manifest, up to limit bytes; false cancels.
 * Immutable ordered source, no reentry. No allocations in the writer. */
typedef bool (*ic64_source)(void *,uint64_t,uint8_t *,size_t,size_t *);
enum ic64_status ic64_mount(struct ic64_state *,struct bs_service *,uint64_t owner,uint64_t token,
 uint64_t sectors,uint64_t capacity,uint64_t data_start,struct ic64_workspace *);
#ifdef STYLEFS_HOST_FORMAT
enum ic64_status ic64_provision(struct bs_service *,uint64_t owner,uint64_t token,
 uint64_t sectors,uint64_t capacity,uint64_t data_start);
#endif
enum ic64_status ic64_publish(struct ic64_state *,struct ic64_workspace *,uint64_t count,ic64_source,void *);
/* Exact retained snapshot for read-only recovery, no writable rollback. */
enum ic64_status ic64_mount_epoch(struct ic64_state *,struct bs_service *,uint64_t owner,uint64_t token,
 uint64_t sectors,uint64_t capacity,uint64_t data_start,uint64_t epoch,struct ic64_workspace *);
#endif
