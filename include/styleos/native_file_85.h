#ifndef STYLEOS_NATIVE_FILE_85_H
#define STYLEOS_NATIVE_FILE_85_H
#include <styleos/stylefs_paged_object64.h>
#include <styleos/user_session_runtime.h>
/* Trusted serialized adapter, not a syscall. Supervisor authorizes exact
 * file revision before grant; never grants arbitrary paths from apps.
 * Dedicated principal namespace high-bit | PID is reserved for this broker
 * in the mounted object service. One broker per service/registry lifetime.
 * All pointer arguments are trusted, disjoint kernel spans, not user VAs.
 * No concurrent call, registry reset or mutable callback into this adapter. */
enum { NF85_HANDLES=8,NF85_READ_MAX=4096 };
struct nf85_owner { uint32_t pid,session;uint64_t handle,root; };
struct nf85_grant { struct nf85_owner owner;uint64_t token,mount;unsigned rights; };
struct nf85_service {
 struct po64_service *fs;
 const struct usr_runtime *sessions;
 struct nf85_grant grants[NF85_HANDLES];
 uint8_t scratch[NF85_READ_MAX];
 bool ready,busy;
};
enum po64_status nf85_start(struct nf85_service *,struct po64_service *,const struct usr_runtime *);
enum po64_status nf85_grant_read(struct nf85_service *,uint32_t pid,uint64_t handle,uint64_t root,
 const struct sd64_key *,uint64_t id,uint64_t revision,uint64_t *token);
/* Supervisor authorizes READ and/or WRITE on one existing revision.
 * No directory mutation, implicit path grants or automatic reauthorization. */
enum po64_status nf85_grant_access(struct nf85_service *,uint32_t pid,uint64_t handle,uint64_t root,
 const struct sd64_key *,uint64_t id,uint64_t revision,unsigned rights,uint64_t *token);
/* Whole-file bounded read, never truncate; output/count unchanged on failure.
 * No byte is released before complete underlying checksum verification.
 * PO64_FULL means insufficient bounded capacity; later streaming API is separate. */
enum po64_status nf85_read(struct nf85_service *,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,uint8_t *out,size_t capacity,size_t *bytes);
/* Bounded whole-file replacement, including empty files. Source is copied
 * to private scratch before durable transaction. On success all old object
 * grants expire; supervisor must explicitly authorize the new revision.
 * On uncertain publication fail closed, never retry automatically. */
enum po64_status nf85_write(struct nf85_service *,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,const uint8_t *data,size_t bytes);
/* Trusted synchronous streaming bridge, NOT an app callback or syscall.
 * Chunk length <= NF85_READ_MAX, 64-bit offsets, callback cannot retain
 * scratch pointer, modify broker/registry/storage, or call user code.
 * Read bytes are provisional until return PO64_OK (checksum verified).
 * Receiver must discard staged output on any failure/cancel. Write uses
 * existing transactional staging, with no publication on cancellation.
 * This contract does NOT bound duration of the full operation; future
 * scheduler-safe asynchronous service must not run it in a trap handler. */
enum po64_status nf85_stream_read(struct nf85_service *,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,pp64_sink,void *context,uint64_t *completed);
enum po64_status nf85_stream_write(struct nf85_service *,uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,uint64_t bytes,pp64_source,void *context);
enum po64_status nf85_close(struct nf85_service *,uint32_t pid,uint64_t handle,uint64_t root,uint64_t token);
/* Trusted periodic/termination reconciliation; dead or rebound owners,
 * revoked object grants and remounts are removed. Lock/switch only hides
 * live grants; it does not destroy them. */
size_t nf85_reconcile(struct nf85_service *);
#endif
