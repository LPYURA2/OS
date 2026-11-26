#ifndef STYLEOS_NAMESPACE64_H
#define STYLEOS_NAMESPACE64_H
#include <styleos/stylefs_dircommit64.h>
/* Namespace policy candidate: virtual root ID 1, one directory entry per
 * object ID, no hard links or symlinks. Object IDs are not capabilities.
 * Caller owns serialized mounted snapshot and trusted nonaliasing buffers.
 * Graph validation uses caller-sized O(n) inventory, no internal heap.
 * Not a proof of inode existence, data ownership, permissions or file CRC. */
enum { NS64_ROOT=1 };
enum ns64_status { NS64_OK, NS64_INVALID, NS64_FULL, NS64_CORRUPT, NS64_IO,
 NS64_STALE, NS64_MISSING, NS64_CONFLICT, NS64_NOT_DIRECTORY, NS64_NOT_EMPTY, NS64_CYCLE, NS64_UNCERTAIN };
struct ns64_node {uint64_t id,parent;uint8_t kind,mark;};
struct ns64_proof {
 const struct dc64_state *source;struct ns64_node *nodes;
 uint64_t epoch,token,count;bool valid;
};
/* Read each leaf once (via ordinal API); sort IDs and validate every parent
 * edge and reachability from root. No proof on failure. Inventory is scratch
 * until success, then immutable until next validation. */
enum ns64_status ns64_validate(struct dc64_state *,struct dc64_workspace *,
 struct ns64_node *,size_t slots,struct ns64_proof *);
/* Absolute, explicit byte-length path; '/' root, exact UTF-8 names.
 * Reject dot components, empty components, trailing slash except root.
 * Output only on success. Caller separately enforces access policy. */
enum ns64_status ns64_resolve(struct dc64_state *,const struct ns64_proof *,
 const uint8_t *path,size_t length,uint8_t scratch[SD64_PAGE],uint64_t *id,uint8_t *kind);
/* Checked single edit; validates unique object, parent type, empty delete
 * and directory ancestry before invoking the proven atomic snapshot writer.
 * Successful mutation invalidates proof; rebuild before next operation.
 * Raw dc64 APIs remain internal; batch graph edits are not exposed here. */
enum ns64_status ns64_apply(struct dc64_state *,struct ns64_proof *,
 struct dc64_workspace *writer,struct dc64_workspace *reader,const struct dc64_edit *);
#endif
