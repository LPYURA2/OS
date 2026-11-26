#ifndef STYLEOS_STREAM64_H
#define STYLEOS_STREAM64_H
#include <styleos/stylefs_alloc64.h>
#include <styleos/stylefs_journal64.h>
/* Trusted serialized prototype: exactly one writer on an exclusively
 * granted, mounted journal. No raw writes or remount while a writer runs.
 * Zero initialize once; never copy an active transaction. All buffers are
 * trusted disjoint spans. File identity is not a userspace capability.
 * The 4KiB metadata bank is still a prototype resource limit. */
enum { SFS64_STREAM_RUNS=SFS64_BANK_BYTES/SFS64_MAP_RECORD };
struct sfs64_stream {
    struct sfs64_journal *journal;
    uint64_t token,generation,received;
    struct sfs64_file file;
    uint32_t running_crc;
    size_t count,prefix,manifest;
    bool active;
    struct sfs64_run occupied[2][SFS64_STREAM_RUNS],runs[SFS64_STREAM_RUNS];
    uint8_t candidate[SFS64_BANK_BYTES];
};
/* expected_revision=0 creates an unused ID; otherwise exact replacement.
 * Protect both retained snapshots before any payload writes. */
enum sj_status sfs64_stream_begin(struct sfs64_stream *,struct sfs64_journal *,
                                  uint64_t id,uint64_t expected_revision,uint64_t bytes);
/* Sequential append, <=32KiB; aligned except EOF. Readback includes zero
 * padding. On I/O uncertainty invalidate transaction and journal.
 * No file visibility before successful finish; cancellation keeps source. */
enum sj_status sfs64_stream_append(struct sfs64_stream *,const uint8_t *,size_t);
/* Only explicitly attested batch-capable data backends. Same readback,
 * checksum and publication contract as append; legacy path unchanged. */
enum sj_status sfs64_stream_append_batched(struct sfs64_stream *,const uint8_t *,size_t);
enum sj_status sfs64_stream_finish(struct sfs64_stream *);
void sfs64_stream_cancel(struct sfs64_stream *);
/* Lookup copies file metadata, not an unsafe long-lived map pointer.
 * Read one bounded chunk from exact id/revision. Returned bytes provisional
 * until caller verifies full content CRC. No ACL or directory service yet. */
enum sj_status sfs64_stream_stat(struct sfs64_journal *,uint64_t id,struct sfs64_file *);
enum bs_status sfs64_stream_read(struct sfs64_journal *,uint64_t id,uint64_t revision,
                                uint64_t offset,uint8_t *,size_t wanted,size_t *completed);
#endif
