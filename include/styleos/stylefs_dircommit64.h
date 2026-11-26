#ifndef STYLEOS_DIRCOMMIT64_H
#define STYLEOS_DIRCOMMIT64_H
#include <styleos/stylefs_directory64.h>
/* Dedicated two-bank metadata slice. Sector 0 and 1 are root receipts.
 * Remaining sectors comprise two equal page-aligned banks. No data extents
 * or ordinary filesystem formats are touched. Trusted serialized caller
 * owns sole write grant; explicitly provisioned blank region only.
 * A new complete sorted snapshot is built into the inactive bank. */
enum dc64_status { DC64_OK, DC64_INVALID, DC64_CORRUPT, DC64_IO, DC64_FULL, DC64_UNCERTAIN, DC64_CANCELLED, DC64_MISSING, DC64_CONFLICT, DC64_STALE };
struct dc64_state {
 struct bs_service *block;uint64_t owner,token,sectors,epoch,count;
 unsigned active;bool valid;
 struct sd64_root root;
};
struct dc64_workspace {
 uint8_t page[SD64_PAGE];
 struct sd64_record records[SD64_FANOUT],child[SD64_FANOUT];
};
typedef bool (*dc64_next)(void *,uint64_t,struct sd64_record *);
/* Mount validates both committed trees, including ordering and exact
 * deterministic child layout. Read failure is not evidence of corruption. */
enum dc64_status dc64_mount(struct dc64_state *,struct bs_service *,uint64_t owner,
                             uint64_t token,uint64_t sectors,struct dc64_workspace *);
/* Blank header initialization only; never erase or autoformat on mount. */
#ifdef STYLEFS_HOST_FORMAT
enum dc64_status dc64_provision(struct bs_service *,uint64_t owner,uint64_t token,uint64_t sectors);
#endif
/* next yields sorted file/directory records by index, false cancels.
 * Caller serializes and keeps the source immutable for entire operation.
 * On I/O uncertainty remount is required. No live page is overwritten. */
enum dc64_status dc64_publish(struct dc64_state *,struct dc64_workspace *,
                               uint64_t count,dc64_next,void *);

/* Read one entry by ordinal, with exact epoch. Workspace/output disjoint.
 * No allocation; 8 sector reads. Missing is distinct from corruption.
 * A corrupt or unreadable current page invalidates the mounted state. */
enum dc64_status dc64_entry_at(struct dc64_state *,struct dc64_workspace *,
                              uint64_t epoch,uint64_t ordinal,struct sd64_record *);
/* Atomic insert/delete/rename in the namespace snapshot; exact old key,
 * expected target and kind for delete/rename. No inode ownership or parent
 * validation here. Caller serializes and verifies those independently.
 * A rename keeps target/kind and refuses occupied destination.
 * Two disjoint workspaces, no heap; still full-snapshot rebuild. */
enum dc64_edit_kind { DC64_INSERT, DC64_DELETE, DC64_RENAME };
struct dc64_edit {
 enum dc64_edit_kind op;
 struct sd64_record entry;
 struct sd64_key destination;
};
enum dc64_status dc64_apply(struct dc64_state *,struct dc64_workspace *writer,
                           struct dc64_workspace *reader,uint64_t epoch,
                           const struct dc64_edit *);

/* Up to 32 independent edits applied in one durable snapshot publication.
 * All edits refer to the same immutable epoch. Existing destinations are
 * refused even if deleted by the batch; dependent edits/swaps not supported.
 * No heap, bounded trusted workspace, preflight before disk writes.
 * Cancel callback is optional, trusted, nonreentrant; true requests stop.
 * Cancellation before publication never changes current directory. */
enum { DC64_BATCH_MAX=32 };
typedef bool (*dc64_cancel)(void *);
enum dc64_status dc64_apply_batch(struct dc64_state *,struct dc64_workspace *writer,
 struct dc64_workspace *reader,uint64_t epoch,const struct dc64_edit *,unsigned count,
 dc64_cancel,void *);
#endif
