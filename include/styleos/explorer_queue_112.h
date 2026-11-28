#ifndef STYLEOS_EXPLORER_QUEUE_112_H
#define STYLEOS_EXPLORER_QUEUE_112_H
#include <styleos/explorer_scope_111.h>
/* Single supervisor-owned queue; no user addresses. Scope lifetime must
 * outlive request; close/reopen only when idle. Serial never reset.
 * Submit/take memory-only; pump may perform synchronous metadata I/O. */
enum { EX112_WIRE_BYTES=296 };
struct ex112_queue {
 struct ex111_scope scope;
 uint64_t serial,id,ordinal;
 unsigned op,state;
 enum po64_status result;
 uint8_t wire[EX112_WIRE_BYTES];
};
enum po64_status ex112_submit(struct ex112_queue *,uint32_t,uint64_t,uint64_t,unsigned,uint64_t,uint64_t *);
bool ex112_pump(struct ex112_queue *);
enum po64_status ex112_take(struct ex112_queue *,uint32_t,uint64_t,uint64_t,uint64_t,uint8_t *,size_t);
void ex112_discard(struct ex112_queue *); /* trusted process retirement; retains serial */
#endif
