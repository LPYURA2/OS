#ifndef STYLEOS_PAGED_PAIR64_H
#define STYLEOS_PAGED_PAIR64_H
#include <styleos/stylefs_dircommit64.h>
#include <styleos/stylefs_inode_root64.h>
#include <styleos/stylefs_alloc64.h>
/* Separate candidate coordinated paged format. Four disjoint slices on
 * one device; data extents are relative to the data grant, not inode
 * metadata. Trusted serialized supervisor owns all staging and protects
 * both retained snapshots' extents. At most one unpublished epoch per
 * component. No raw restaging after uncertainty. Read-only recovery;
 * guarded data allocation and rollback are separate future work. */
enum pp64_status { PP64_OK,PP64_INVALID,PP64_CORRUPT,PP64_IO,PP64_FULL,PP64_UNCERTAIN,PP64_CANCELLED };
struct pp64_layout {uint64_t owner,receipt,dir,inode,data,dir_sectors,inode_sectors,data_sectors,data_start;};
struct pp64_pair {uint64_t sequence,dir_epoch,inode_epoch;unsigned slot;};
struct pp64_workspace {
 struct dc64_state dir;
 struct dc64_edit_workspace dw;
 struct ir64_state inode;
 struct ip64_workspace iw;
};
enum pp64_status pp64_mount(struct bs_service *,const struct pp64_layout *,struct pp64_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *,size_t run_cap,struct pp64_pair *);
enum pp64_status pp64_commit(struct bs_service *,const struct pp64_layout *,struct pp64_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *,size_t run_cap,
 uint64_t expected_sequence,uint64_t dir_epoch,uint64_t inode_epoch,struct pp64_pair *);
/* Recover a consistent selected pair before staging any new transaction.
 * Retire dangling global receipt before discarding unpublished roots.
 * Requires all four exclusive RW grants; never clears selected root.
 * On uncertainty stop and cold reopen. No raw writer or concurrent call. */
enum pp64_status pp64_prepare(struct bs_service *,const struct pp64_layout *,struct pp64_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *,size_t run_cap,struct pp64_pair *);
/* Read-only plan for a prepared pair. Protect union of current and prior
 * retained inode roots. Each scratch array has run_cap entries, disjoint.
 * Refuses unpublished newer roots, stale sequence and insufficient slots.
 * No writes/reservation; serialized supervisor must retain exclusivity
 * through subsequent data staging and publication. */
enum pp64_status pp64_plan(struct bs_service *,const struct pp64_layout *,struct pp64_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *current,struct sfs64_run *previous,size_t run_cap,
 uint64_t expected_sequence,uint64_t bytes,struct sfs64_run *out,size_t slots,size_t *count);
/* Trusted synchronous streaming supervisor; no reentrant callbacks or
 * competing writers. File IDs must be lifetime-unique. One map per file
 * fits one page; more fragmented plans return FULL. Buffers are disjoint.
 * Source failure cancels before publishing. Sink receives provisional
 * bytes and MUST discard on anything other than PP64_OK (CRC verified).
 * Exact sequence and revision guard stale requests. */
enum { PP64_FILE_RUNS=(IP64_MAP_MAX-SFS64_MAP_HEADER)/SFS64_MAP_RECORD };
struct pp64_stream_workspace {
 struct pp64_workspace pair;
 struct ip64_workspace source;
 struct sfs64_run plan[PP64_FILE_RUNS];
 uint8_t transfer[SFS64_WINDOW_BYTES],manifest[IP64_MAP_MAX];
};
typedef bool (*pp64_source)(void *,uint64_t,uint8_t *,size_t);
typedef bool (*pp64_sink)(void *,uint64_t,const uint8_t *,size_t);
enum pp64_status pp64_put(struct bs_service *,const struct pp64_layout *,struct pp64_stream_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *current,struct sfs64_run *previous,size_t run_cap,
 uint64_t expected,const struct sd64_key *,uint64_t id,uint64_t revision,uint64_t bytes,
 pp64_source,void *,struct pp64_pair *);
enum pp64_status pp64_get(struct bs_service *,const struct pp64_layout *,struct pp64_stream_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *,size_t run_cap,
 uint64_t expected,const struct sd64_key *,uint64_t id,uint64_t revision,pp64_sink,void *);
/* Guarded mkdir, rename/move, empty rmdir and unlink. File deletion
 * retires its map and name in one paired commit; no secure erase. The
 * exact sequence and revision must match. Directory revision is zero.
 * Caller supplies lifetime-unique IDs and serializes exclusive grants. */
enum pp64_status pp64_edit(struct bs_service *,const struct pp64_layout *,struct pp64_stream_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *,size_t run_cap,
 uint64_t expected,const struct dc64_edit *,uint64_t revision,struct pp64_pair *);
#ifdef STYLEFS_HOST_FORMAT
enum pp64_status pp64_provision(struct bs_service *,const struct pp64_layout *,struct pp64_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *,size_t run_cap,struct pp64_pair *);
#endif
#endif
