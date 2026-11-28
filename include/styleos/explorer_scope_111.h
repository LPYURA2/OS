#ifndef STYLEOS_EXPLORER_SCOPE_111_H
#define STYLEOS_EXPLORER_SCOPE_111_H
#include <styleos/native_file_85.h>
/* Trusted serialized adapter; supervisor authorizes directory and identity.
 * No raw user pointers, public syscall, or authority from supplied IDs.
 * Any mutation/remount invalidates the snapshot. */
enum { EX111_DEPTH=16 };
struct ex111_scope {
 struct po64_service *fs;
 const struct usr_runtime *users;
 struct nf85_owner owner;
 struct po64_snapshot snapshot;
 uint64_t dirs[EX111_DEPTH];
 unsigned depth;
 bool active,edit_allowed;
};
enum po64_status ex111_open(struct ex111_scope *,struct po64_service *,const struct usr_runtime *,uint32_t,uint64_t,uint64_t,uint64_t);
enum po64_status ex111_row(struct ex111_scope *,uint32_t,uint64_t,uint64_t,uint64_t,struct po64_row *);
enum po64_status ex111_enter(struct ex111_scope *,uint32_t,uint64_t,uint64_t,uint64_t);
enum po64_status ex111_up(struct ex111_scope *,uint32_t,uint64_t,uint64_t);
bool ex111_authorized(const struct ex111_scope *,uint32_t,uint64_t,uint64_t);
/* Supervisor-only explicit namespace permission on ID-managed RW volume.
 * Edit takes exact observed object identity, current ordinal and name.
 * Move destination is an already admitted ancestor index, not a raw ID.
 * No overwrite, recursive deletion, copy, raw block or file-data grant. */
enum ex119_op { EX119_FILE=1, EX119_DIRECTORY, EX119_RENAME, EX119_DELETE, EX119_MOVE_UP };
enum po64_status ex119_allow_edit(struct ex111_scope *,uint32_t,uint64_t,uint64_t);
enum po64_status ex119_edit(struct ex111_scope *,uint32_t,uint64_t,uint64_t,
 unsigned op,uint64_t ordinal,uint64_t expected_id,uint64_t expected_revision,
 unsigned ancestor,const uint8_t *name,size_t name_bytes);
/* Revalidate same mount and explicit root; discard old path and ordinals.
 * Only ID-managed volumes allow refresh after external namespace change. */
enum po64_status ex119_refresh(struct ex111_scope *,uint32_t,uint64_t,uint64_t);
void ex111_close(struct ex111_scope *);
#endif
