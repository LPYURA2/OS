#ifndef STYLEOS_INODETABLE64_H
#define STYLEOS_INODETABLE64_H
#include <styleos/stylefs_map64.h>
/* Immutable packed inode/extent-manifest pages, separate candidate format.
 * Metadata grant and file-data geometry are independent; all sector offsets
 * in manifests remain relative to the data grant. Trusted serialized caller
 * owns immutable storage, stable state and disjoint scratch buffers.
 * No allocation, publication, global extent ownership or ACL here. */
enum { IT64_PAGE=4096, IT64_SECTORS=8, IT64_HEADER=64, IT64_MAX_RECORDS=59 };
enum it64_status { IT64_OK, IT64_INVALID, IT64_CORRUPT, IT64_IO, IT64_MISSING, IT64_STALE };
struct it64_blob {const uint8_t *bytes;size_t length;};
struct it64_geometry {uint64_t epoch,start,pages,capacity,data_start;};
struct it64_state {
 struct bs_service *block;
 uint64_t owner,token,count;
 struct it64_geometry geometry;
 bool valid;
};
/* Sorted nonempty canonical packed page. Input blobs are already encoded
 * SF64MAP1 manifests; validation before output. Empty table has zero pages.
 * A manifest must fit entirely in one page, no spill-chain support yet. */
bool it64_encode(uint8_t page[IT64_PAGE],const struct it64_geometry *,uint64_t ordinal,
 const struct it64_blob *,unsigned count);
/* Validate every page and globally strict object ID order with one page
 * scratch. No autoformat; does not verify file content or cross-file extents. */
enum it64_status it64_mount(struct it64_state *,struct bs_service *,uint64_t owner,uint64_t token,
 const struct it64_geometry *,uint8_t scratch[IT64_PAGE]);
/* Binary search packed pages. Returned map.records borrows scratch until
 * next scratch use. Outputs unchanged on error; scratch may be overwritten.
 * Refuse stale epoch/grant, invalidate state on media/canonical errors. */
enum it64_status it64_lookup(struct it64_state *,uint64_t epoch,uint64_t id,
 uint8_t scratch[IT64_PAGE],struct sfs64_map *);

/* Scan all maps in a mounted immutable table. Each view borrows scratch
 * during the callback only. No reentry/mutation; false stops the scan. */
typedef bool (*it64_visit)(void *,const struct sfs64_map *);
enum it64_status it64_walk(struct it64_state *,uint8_t scratch[IT64_PAGE],it64_visit,void *);
#endif
