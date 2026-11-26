#ifndef STYLEOS_WIDEVOLUME64_H
#define STYLEOS_WIDEVOLUME64_H
#include <styleos/stylefs_namespace64.h>
#include <styleos/stylefs_inodestream64.h>
/* Isolated candidate format with four exclusive, disjoint grant slices.
 * A supervisor owns stable state/inventory/workspaces, serialized calls,
 * and prevents all raw I/O or callback reentry. Only file creation exposed.
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
};
#ifdef STYLEFS_HOST_FORMAT
enum wv64_status wv64_provision_checkpoint(struct bs_service *,const struct wv64_layout *,struct wv64_workspace *);
#endif
enum wv64_status wv64_open(struct wv64_state *,struct bs_service *,const struct wv64_layout *,
 bool write,struct ns64_node *,size_t,struct wv64_workspace *,struct ow64_inventory banks[2]);
enum wv64_status wv64_create(struct wv64_state *,struct wv64_workspace *,struct ow64_inventory banks[2],
 const struct sd64_record *,uint64_t bytes,is64_source,void *);
enum wv64_status wv64_stat(struct wv64_state *,struct wv64_workspace *,const uint8_t *,size_t,struct sfs64_file *);
#endif
