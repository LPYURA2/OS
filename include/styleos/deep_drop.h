#ifndef STYLEOS_DEEP_DROP_H
#define STYLEOS_DEEP_DROP_H
#include <styleos/native_window_channel.h>
/* Trusted serialized kernel objects only. Architecture must authenticate
 * source/target calls and user gesture before invoking. No CPL3 ABI yet. */
enum { DD_TEXT_MAX=1024 };
enum dd_status { DD_OK, DD_INVALID, DD_DENIED, DD_BUSY, DD_STALE, DD_CAPACITY, DD_EXHAUSTED };
enum dd_phase { DD_IDLE, DD_OFFERED, DD_ACCEPTED, DD_DELIVERED };
struct dd_endpoint { uint32_t pid,window; uint64_t generation,root; };
struct dd_transfer {
 enum dd_phase phase;
 uint64_t id,deadline;
 struct dd_endpoint source,target;
 size_t size;
 uint8_t text[DD_TEXT_MAX];
};
struct dd_service { struct nwc_service *channel; uint64_t last_id; struct dd_transfer active; };
/* One fixed slot, zero-initialized service, no heap or source pointer retained.
 * now/deadline use a trusted monotonic clock, not caller-provided time.
 * All buffers must be distinct trusted kernel spans. */
enum dd_status dd_init(struct dd_service *,struct nwc_service *);
enum dd_status dd_offer(struct dd_service *,struct dd_endpoint,struct dd_endpoint,
 const uint8_t *,size_t,uint64_t now,uint64_t deadline,bool gesture,uint64_t *id);
enum dd_status dd_accept(struct dd_service *,struct dd_endpoint,uint64_t id,uint64_t now);
enum dd_status dd_deliver(struct dd_service *,struct dd_endpoint,uint64_t id,uint64_t now,
 uint8_t *out,size_t capacity,size_t *written);
enum dd_status dd_finish(struct dd_service *,struct dd_endpoint,uint64_t id,uint64_t now);
enum dd_status dd_cancel(struct dd_service *,struct dd_endpoint,uint64_t id);
/* Scrub on expiry or lost endpoint authority; true iff a transfer was aborted. */
bool dd_reconcile(struct dd_service *,uint64_t now);
#endif
