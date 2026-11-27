#ifndef STYLEOS_FILEBROKER64_H
#define STYLEOS_FILEBROKER64_H
#include <styleos/stylefs_volumemount64.h>
/* Trusted kernel-side broker prototype, not a syscall ABI.
 * Issue/revoke are privileged supervisor operations; principal is supplied
 * by authenticated process lifetime, never by an untrusted request.
 * Caller serializes operations, owns stable mount and workspace objects,
 * and passes validated kernel buffers (not CPL3 pointers).
 * One exact selected file; read by default, explicit save grant only.
 * No directory or raw block authority.
 * All tokens conservatively expire on any committed mutation or remount.
 * Zero-initialize broker once; do not copy or reset its token counter.
 */
enum { FBK64_SLOTS=16, FBK64_PATH_MAX=1024 };
struct fbk64_slot {
 struct vm64_mount *mount;
 uint64_t token,principal,receipt,sequence,id,revision;
 bool save;
 size_t length;
 uint8_t path[FBK64_PATH_MAX];
};
struct fbk64_broker {uint64_t serial;struct fbk64_slot slots[FBK64_SLOTS];};
enum wv64_status fbk64_issue(struct fbk64_broker *,struct vm64_mount *,struct wv64_workspace *,
 uint64_t principal,const uint8_t *path,size_t length,uint64_t *token);
enum wv64_status fbk64_read(struct fbk64_broker *,struct vm64_mount *,struct wv64_workspace *,
 uint64_t principal,uint64_t token,uint64_t offset,uint8_t *,size_t,size_t *completed);
enum wv64_status fbk64_revoke(struct fbk64_broker *,uint64_t principal,uint64_t token);
enum wv64_status fbk64_revoke_principal(struct fbk64_broker *,uint64_t principal);
/* Explicit read+save grant. One bounded kernel-buffer replacement, including
 * empty file; success invalidates all tokens of the old mount sequence.
 * No user pointers, no callback from the requesting process. */
enum wv64_status fbk64_issue_save(struct fbk64_broker *,struct vm64_mount *,struct wv64_workspace *,
 uint64_t,const uint8_t *,size_t,uint64_t *);
enum wv64_status fbk64_save(struct fbk64_broker *,struct vm64_mount *,struct wv64_workspace *,
 struct ow64_inventory banks[2],uint64_t principal,uint64_t token,const uint8_t *,size_t);
#endif
