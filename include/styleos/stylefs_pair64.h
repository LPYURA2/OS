
#ifndef STYLEOS_PAIR64_H
#define STYLEOS_PAIR64_H
#include <styleos/stylefs_dircommit64.h>
#include <styleos/stylefs_stream64.h>
/* Experimental coordinated visibility receipt, separate two-sector slice.
 * All three exact, disjoint grants are on the same device, same owner.
 * Single trusted supervisor; no raw user access or concurrent writer.
 * Stage at most ONE new version per component after a committed pair.
 * Do not restage, allocate or use standalone latest mounts after uncertain
 * results. Recovery here is read-only; repair/retry supervisor is future.
 * Directory/inode format unchanged; no automatic format or data GC. */
enum sp64_status { SP64_OK,SP64_INVALID,SP64_CORRUPT,SP64_IO,SP64_FULL,SP64_UNCERTAIN,SP64_CANCELLED };
struct sp64_layout {uint64_t owner,receipt,dir,data,dir_sectors,data_sectors,data_start;};
struct sp64_pair {uint64_t sequence,dir_epoch,data_epoch;unsigned slot;};
struct sp64_workspace {
 struct dc64_state dir;
 struct dc64_edit_workspace dw;
 struct sfs64_journal data;
};
/* On success selected components in workspace are read-only views.
 * Do not pass them to raw mutation APIs. Outputs undefined on failure. */
enum sp64_status sp64_mount(struct bs_service *,const struct sp64_layout *,
 struct sp64_workspace *,struct dc64_node *,size_t cap,struct sp64_pair *);
/* Verify the current committed pair still exists, validate proposed pair
 * including exact bijection of file names and manifests, then publish a
 * single durable receipt. New epochs may advance by at most one each. */
enum sp64_status sp64_commit(struct bs_service *,const struct sp64_layout *,
 struct sp64_workspace *,struct dc64_node *,size_t cap,
 uint64_t expected_sequence,uint64_t dir_epoch,uint64_t data_epoch,struct sp64_pair *);
/* Serialized recovery/preparation gate. Discard uncommitted newer component
 * receipts (never the selected pair), invalidate a dangling newer global
 * receipt before reusing its epochs. Refuse jumps greater than one epoch.
 * No data sectors erased; allocation may reclaim abandoned extents.
 * On uncertainty stop and cold reopen; never retry in the same session.
 * Call only with sole RW grants for all three slices, no active raw writer.
 */
enum sp64_status sp64_prepare(struct bs_service *,const struct sp64_layout *,
 struct sp64_workspace *,struct dc64_node *,size_t cap,struct sp64_pair *);
/* Single synchronous guarded create/replace. Source fills exact requested
 * chunk, false cancels before publication. Callbacks cannot reenter storage.
 * Caller provides disjoint workspace, stream and 32KiB transfer buffer.
 * Zero revision creates; existing revision replaces exact key and object.
 * Failed operations may leave unpublished staging; next prepare discards it.
 */
typedef bool (*sp64_source)(void *,uint64_t offset,uint8_t *,size_t);
struct sp64_put_workspace {
 struct sp64_workspace pair;
 struct sfs64_stream stream;
 uint8_t transfer[SFS64_WINDOW_BYTES];
};
enum sp64_status sp64_put_file(struct bs_service *,const struct sp64_layout *,
 struct sp64_put_workspace *,struct dc64_node *,size_t cap,const struct sd64_key *,
 uint64_t id,uint64_t revision,uint64_t bytes,sp64_source,void *,struct sp64_pair *);
/* Guarded namespace edits. CREATE only directory; REMOVE file/directory;
 * RENAME moves either kind. Exact committed sequence and file revision
 * guard stale requests. File deletion removes its manifest in the same
 * paired transaction; retained snapshot extents remain protected.
 * Caller allocates lifetime-unique object IDs, no recursive deletion,
 * no secure erase. Scratch is disjoint and trusted. */
struct sp64_edit_workspace {
 struct sp64_workspace pair;
 uint8_t payload[SFS64_BANK_BYTES];
};
enum sp64_status sp64_edit(struct bs_service *,const struct sp64_layout *,
 struct sp64_edit_workspace *,struct dc64_node *,size_t cap,
 uint64_t expected_sequence,const struct dc64_edit *,uint64_t expected_revision,
 struct sp64_pair *);
#ifdef STYLEFS_HOST_FORMAT
enum sp64_status sp64_provision(struct bs_service *,const struct sp64_layout *,
 struct sp64_workspace *,struct dc64_node *,size_t cap,struct sp64_pair *);
#endif
#endif
