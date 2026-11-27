#ifndef STYLEOS_PAGED_OBJECT64_H
#define STYLEOS_PAGED_OBJECT64_H
#include <styleos/stylefs_paged_pair64.h>
/* Kernel-only read-only object broker. Supervisor supplies exclusive
 * stable read-only block layout and authenticated lifetime-unique
 * principal. No raw grants, path discovery or user callbacks in CPL3.
 * Zero-init once; never copy/reset service or reuse its serial. Mount
 * borrows grants/scratch; supervisor retains ownership until unmount.
 * No concurrent calls except callback reentry (explicitly refused). */
enum { PO64_HANDLES=16 };
enum po64_status { PO64_OK,PO64_INVALID,PO64_DENIED,PO64_STALE,PO64_BUSY,PO64_FULL,PO64_IO,PO64_CORRUPT,PO64_CANCELLED };
struct po64_handle {uint64_t token,principal,id,revision;struct sd64_key key;};
struct po64_service {
 uint64_t serial;bool active,busy;
 struct bs_service *block;struct pp64_layout layout;struct pp64_pair pair;
 struct pp64_stream_workspace *work;
 struct dc64_node *nodes;size_t node_cap;
 struct sfs64_run *runs;size_t run_cap;
 struct po64_handle handles[PO64_HANDLES];
};
enum po64_status po64_mount(struct po64_service *,struct bs_service *,const struct pp64_layout *,
 struct pp64_stream_workspace *,struct dc64_node *,size_t,struct sfs64_run *,size_t);
enum po64_status po64_unmount(struct po64_service *);
/* Only supervisor can grant; key and ID/revision verified at grant.
 * A token authorizes exactly one immutable file revision, never parent
 * directory or siblings. Scope does not survive unmount or media loss. */
enum po64_status po64_grant(struct po64_service *,uint64_t principal,const struct sd64_key *,
 uint64_t id,uint64_t revision,uint64_t *token);
enum po64_status po64_revoke(struct po64_service *,uint64_t principal,uint64_t token);
enum po64_status po64_revoke_owner(struct po64_service *,uint64_t principal);
/* Sink receives provisional bytes; publish destination only on PO64_OK.
 * Kernel callback must not mutate trusted storage or reenter broker. */
enum po64_status po64_read(struct po64_service *,uint64_t principal,uint64_t token,pp64_sink,void *);
#endif
