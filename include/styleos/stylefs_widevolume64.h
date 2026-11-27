#ifndef STYLEOS_WIDEVOLUME64_H
#define STYLEOS_WIDEVOLUME64_H
#include <styleos/stylefs_namespace64.h>
#include <styleos/stylefs_inodestream64.h>
/* Isolated candidate format with four exclusive, disjoint grant slices.
 * A supervisor owns stable state/inventory/workspaces, serialized calls,
 * and prevents all raw I/O or callback reentry. File creation and checked namespace edits exposed.
 * Mount never formats; explicit host initial checkpoint on empty pair.
 * Read-only recovery selects an exact retained pair. Writable open refuses
 * pending/mismatching roots; explicit reconciliation is a later stage. */
struct wv64_layout {uint64_t owner,receipt,dir,inode,data,dir_sectors,inode_sectors,data_sectors,data_start;};
enum wv64_status {WV64_OK,WV64_INVALID,WV64_LOCKED,WV64_FULL,WV64_CONFLICT,WV64_CANCELLED,
 WV64_MISSING,WV64_IO,WV64_CORRUPT,WV64_RECOVERY};
struct wv64_state {
 struct bs_service *block;struct wv64_layout layout;
 struct dc64_state dir;struct ic64_state inode;struct ns64_proof proof;
 struct ns64_node *nodes;size_t slots;
 uint64_t sequence,dir_epoch,inode_epoch;
 bool ready,write,busy,fallback;
};
struct wv64_workspace {
 struct dc64_workspace writer,reader;
 struct is64_workspace stream;
 uint8_t copy_page[IT64_PAGE];
};
#ifdef STYLEFS_HOST_FORMAT
enum wv64_status wv64_provision_checkpoint(struct bs_service *,const struct wv64_layout *,struct wv64_workspace *);
#endif
enum wv64_status wv64_open(struct wv64_state *,struct bs_service *,const struct wv64_layout *,
 bool write,struct ns64_node *,size_t,struct wv64_workspace *,struct ow64_inventory banks[2]);
enum wv64_status wv64_create(struct wv64_state *,struct wv64_workspace *,struct ow64_inventory banks[2],
 const struct sd64_record *,uint64_t bytes,is64_source,void *);
enum wv64_status wv64_stat(struct wv64_state *,struct wv64_workspace *,const uint8_t *,size_t,struct sfs64_file *);
/* Checked mkdir, move/rename, and delete (empty directories or files).
 * Exact expected ID/kind/key; no recursive removal or overwrite.
 * Object IDs are supplied by trusted supervisor, not user capabilities. */
enum wv64_status wv64_edit(struct wv64_state *,struct wv64_workspace *,struct ow64_inventory banks[2],const struct dc64_edit *);
/* Epoch-scoped directory cursor. Enumerates direct children only.
 * A cursor is valid for the same mounted session/sequence and parent.
 * No recursive scan, no callback, at most 14 entries per call. */
struct wv64_cursor {uint64_t sequence,parent,ordinal,token;};
enum wv64_status wv64_list_open(struct wv64_state *,struct wv64_workspace *,const uint8_t *,size_t,struct wv64_cursor *);
enum wv64_status wv64_list_next(struct wv64_state *,struct wv64_workspace *,struct wv64_cursor *,
 struct sd64_record out[SD64_FANOUT],unsigned *count);
/* Bounded read (up to 32 KiB), no output on error. Short read at EOF.
 * expected ID/revision guard stale file selection; no whole-file CRC promise
 * for arbitrary partial reads. Copy verifies the entire original CRC. */
enum wv64_status wv64_read(struct wv64_state *,struct wv64_workspace *,const uint8_t *,size_t,
 uint64_t id,uint64_t revision,uint64_t offset,uint8_t *,size_t,size_t *);
typedef bool (*wv64_progress)(void *,uint64_t completed,uint64_t total);
/* Same-volume single-file copy, no overwrite or source deletion.
 * Progress callback returns false to cancel, cannot reenter the volume.
 * Completion is acknowledged only after checkpoint publication. */
enum wv64_status wv64_copy(struct wv64_state *,struct wv64_workspace *,struct ow64_inventory banks[2],
 const uint8_t *path,size_t length,uint64_t id,uint64_t revision,const struct sd64_record *destination,
 wv64_progress,void *);
/* Explicit repair under exclusive trusted supervision. Fresh attachment
 * required after uncertain I/O. Newest common receipt must be intact;
 * never fall back for writes or adopt a pending root. Only uncommitted
 * newer metadata headers may be cleared. No file-content writes. */
enum wv64_status wv64_reconcile(struct wv64_state *,struct bs_service *,const struct wv64_layout *,
 struct ns64_node *,size_t,struct wv64_workspace *,struct ow64_inventory banks[2]);
/* Exact existing path/id/revision, streamed copy-on-write save.
 * Inode-only checkpoint advances sequence and inode epoch, not directory.
 * Older diagnostic readers that require a directory increment refuse this
 * checkpoint sequence. Never downgrade a written volume without migration. */
enum wv64_status wv64_replace(struct wv64_state *,struct wv64_workspace *,struct ow64_inventory banks[2],
 const uint8_t *,size_t,uint64_t id,uint64_t revision,uint64_t bytes,is64_source,void *);
#endif
