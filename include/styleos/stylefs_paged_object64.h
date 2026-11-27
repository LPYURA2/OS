#ifndef STYLEOS_PAGED_OBJECT64_H
#define STYLEOS_PAGED_OBJECT64_H
#include <styleos/stylefs_paged_pair64.h>
/* Kernel-only scoped object broker. Supervisor supplies exclusive
 * stable read-only or exclusive read-write block layout and authenticated lifetime-unique
 * principal. No raw grants, path discovery or user callbacks in CPL3.
 * Zero-init once; never copy/reset service or reuse its serial. Mount
 * borrows grants/scratch; supervisor retains ownership until unmount.
 * No concurrent calls except callback reentry (explicitly refused). */
enum { PO64_HANDLES=16 };
enum po64_status { PO64_OK,PO64_INVALID,PO64_DENIED,PO64_STALE,PO64_BUSY,PO64_FULL,PO64_IO,PO64_CORRUPT,PO64_CANCELLED,PO64_UNCERTAIN,PO64_END };
enum { PO64_READ=1u, PO64_WRITE=2u };
struct po64_handle {uint64_t token,principal,id,revision;struct sd64_key key;unsigned rights;};
struct po64_service {
 uint64_t serial,mount_id;bool active,busy,writable;
 struct bs_service *block;struct pp64_layout layout;struct pp64_pair pair;
 struct pp64_stream_workspace *work;
 struct dc64_node *nodes;size_t node_cap;
 struct sfs64_run *runs,*previous;size_t run_cap;
 struct po64_handle handles[PO64_HANDLES];
};
enum po64_status po64_mount(struct po64_service *,struct bs_service *,const struct pp64_layout *,
 struct pp64_stream_workspace *,struct dc64_node *,size_t,struct sfs64_run *,size_t);
/* Explicit writable mode requires four RW grants and independent prior
 * extent scratch. Existing read-only mount remains unchanged. */
enum po64_status po64_mount_rw(struct po64_service *,struct bs_service *,const struct pp64_layout *,
 struct pp64_stream_workspace *,struct dc64_node *,size_t,struct sfs64_run *,struct sfs64_run *,size_t);
enum po64_status po64_unmount(struct po64_service *);
/* Only supervisor can grant; key and ID/revision verified at grant.
 * A token authorizes exactly one immutable file revision, never parent
 * directory or siblings. Scope does not survive unmount or media loss. */
enum po64_status po64_grant(struct po64_service *,uint64_t principal,const struct sd64_key *,
 uint64_t id,uint64_t revision,uint64_t *token);
/* Supervisor alone grants rights for the existing exact file. No create,
 * delete, rename or directory authority follows from a write token. */
enum po64_status po64_grant_access(struct po64_service *,uint64_t principal,const struct sd64_key *,
 uint64_t id,uint64_t revision,unsigned rights,uint64_t *token);
enum po64_status po64_revoke(struct po64_service *,uint64_t principal,uint64_t token);
enum po64_status po64_revoke_owner(struct po64_service *,uint64_t principal);
/* Sink receives provisional bytes; publish destination only on PO64_OK.
 * Kernel callback must not mutate trusted storage or reenter broker. */
enum po64_status po64_read(struct po64_service *,uint64_t principal,uint64_t token,pp64_sink,void *);
/* Replace authorized exact revision. A durable commit revokes ALL old
 * handles; supervisor reauthorizes against the new snapshot. Cancellation
 * retains old handles only if the exact committed snapshot revalidates.
 * Uncertain result closes the mount; never retry without recovery.
 * Trusted source only, no user pointers or callback reentry. */
enum po64_status po64_write(struct po64_service *,uint64_t principal,uint64_t token,uint64_t bytes,pp64_source,void *);
/* Supervisor-only directory view for the file picker/Explorer. NOT an
 * application capability: caller must authorize the requested directory.
 * Snapshot references become stale on any mutation or remount. Names
 * are length-delimited UTF-8; all IDs, sizes and revisions are 64 bit.
 * No output changes on failure or end. Root is implicit object 1. */
struct po64_snapshot {uint64_t mount,sequence;};
struct po64_row {
 struct sd64_record entry;
 uint64_t bytes,revision;
};
enum po64_status po64_snapshot(struct po64_service *,struct po64_snapshot *);
enum po64_status po64_child_at(struct po64_service *,struct po64_snapshot,
 uint64_t parent,uint64_t ordinal,struct po64_row *);
#endif
