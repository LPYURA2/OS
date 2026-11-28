#ifndef STYLEOS_EXPLORER_QUEUE_112_H
#define STYLEOS_EXPLORER_QUEUE_112_H
#include <styleos/explorer_scope_111.h>
#include <styleos/explorer_volume_115.h>
/* Single supervisor-owned queue; no user addresses. Scope lifetime must
 * outlive request; close/reopen only when idle. Serial never reset.
 * Submit/take memory-only; pump may perform synchronous metadata I/O. */
enum { EX112_WIRE_BYTES=296, EX120_EDIT_BYTES=320, EX120_RECEIPT_BYTES=24 };
struct ex112_queue {
 struct ex111_scope scope;
 struct pv64_volume *volume; /* trusted visible volume; same object scope */
 struct sfs64_run *volume_ro_scratch;
 uint64_t serial,id,ordinal;
 unsigned op,state;
 enum po64_status result;
 uint8_t wire[EX112_WIRE_BYTES];
 uint8_t edit[EX120_EDIT_BYTES];
};
enum po64_status ex112_submit(struct ex112_queue *,uint32_t,uint64_t,uint64_t,unsigned,uint64_t,uint64_t *);
/* Supervisor binds exact mounted volume after successful scoped admission. */
enum po64_status ex117_bind_volume(struct ex112_queue *,struct pv64_volume *,struct sfs64_run *);
/* Submit copies exact validated command. No I/O or retained user address.
 * Operation 5 is edit; operation 6 is refresh via ordinary submit. */
enum po64_status ex120_submit_edit(struct ex112_queue *,uint32_t,uint64_t,uint64_t,const uint8_t *,size_t,uint64_t *);
bool ex112_pump(struct ex112_queue *);
enum po64_status ex112_take(struct ex112_queue *,uint32_t,uint64_t,uint64_t,uint64_t,uint8_t *,size_t);
void ex112_discard(struct ex112_queue *); /* trusted process retirement; retains serial */
#endif
