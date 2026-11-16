#ifndef STYLEOS_STORAGE_H
#define STYLEOS_STORAGE_H
#include <stddef.h>
#include <stdint.h>

/* T-P1.8A experiment only. Every transfer is one complete 512-byte sector.
 * The callback returns zero only on successful completion. Flush must be a
 * device-confirmed durability operation, not a compiler/memory barrier. */
#define ST_SECTOR_BYTES 512u
#define ST_DISK_SECTORS UINT64_C(2048)
#define ST_NAME_MAX 31u
#define ST_DATA_MAX 1024u
#define ST_WORKSPACE_BYTES 4096u
#define ST_SLOT_COUNT 2u
#define ST_SLOT_SECTORS 4u
#define ST_SLOT0_LBA UINT64_C(2)
#define ST_SLOT1_LBA UINT64_C(6)
struct st_io {
    void *context;
    uint64_t sectors;
    int (*read)(void *, uint64_t lba, void *sector);
    int (*write)(void *, uint64_t lba, const void *sector);
    int (*flush)(void *);
};
enum st_error {
    ST_OK, ST_ARGUMENT, ST_CAPACITY, ST_IO, ST_SIGNATURE, ST_CORRUPT,
    ST_NOT_EMPTY, ST_EMPTY, ST_STATE, ST_RANGE, ST_OVERFLOW
};
struct st_record {
    uint64_t generation;
    uint32_t name_length, length;
    unsigned char name[ST_NAME_MAX + 1u], data[ST_DATA_MAX];
};
struct st_pending {
    uint64_t generation;
    uint32_t slot, length, metadata_crc, payload_crc;
};
struct st_store {
    struct st_io io;
    uint64_t generation;
    int active; /* -1 only immediately after explicit empty initialization. */
    unsigned mounted, prepared;
    struct st_pending pending;
};
/* Caller owns and admits this complete workspace before the first call.
 * No disk pointers, heap, arbitrary PA conversions or boot pool are used. */
union st_workspace {
    struct { unsigned char sector[ST_SECTOR_BYTES], payload[ST_DATA_MAX]; } io;
    unsigned char bytes[ST_WORKSPACE_BYTES];
};
_Static_assert(sizeof(union st_workspace)==ST_WORKSPACE_BYTES,"storage workspace extent");

uint32_t st_crc32(const void *, size_t);
/* Initialization scans the ENTIRE exact-capacity disk for zeroes before any
 * write. It is a separate explicit operation, never a mount fallback. */
enum st_error st_init(const struct st_io *, union st_workspace *, struct st_store *);
/* Mount is strictly read-only. A nonempty disk without a whole committed
 * version fails; a pristine initialized empty store returns ST_EMPTY. */
enum st_error st_mount(const struct st_io *, union st_workspace *, struct st_store *);
enum st_error st_read(struct st_store *, union st_workspace *, struct st_record *);
/* prepare first zeroes the inactive commit and confirms FLUSH before writing
 * replacement metadata + both payload sectors and confirming a second FLUSH.
 * This prevents an old commit from matching repaired bytes after fallback.
 * No new commit is written. The guest may stop permanently at this boundary.
 * A complete save therefore confirms three FLUSH operations. */
enum st_error st_prepare(struct st_store *, union st_workspace *,
    const unsigned char *name, size_t name_length,
    const unsigned char *data, size_t length, struct st_pending *);
/* Only the current successful prepare token is accepted. A successful return
 * follows commit-record write AND final confirmed FLUSH. On I/O uncertainty
 * the store becomes unmounted; caller must not claim durability or retry using
 * stale in-memory state. Disk backing is never freed or reformatted. */
enum st_error st_commit(struct st_store *, union st_workspace *, const struct st_pending *);
const char *st_error_name(enum st_error);
#endif
