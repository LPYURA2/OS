#ifndef STYLEOS_BLOCK_SERVICE_H
#define STYLEOS_BLOCK_SERVICE_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Trusted, synchronous, single-CPU kernel service. Zero-initialized once,
 * never copied or reset during its lifetime. Not a userspace ABI.
 * The architecture adapter must authenticate device identity, bounds and
 * callback ownership before attach, and serialize calls on the safe root.
 * Buffers are validated kernel spans, never direct CPL3 pointers.
 * No automatic probing, format, retry, DMA or new hardware grant. */
enum { BS_SECTOR_BYTES=512, BS_DEVICES=4, BS_GRANTS=16 };
enum bs_status { BS_OK=0, BS_INVALID, BS_DENIED, BS_STALE, BS_BUSY,
                 BS_FULL, BS_RANGE, BS_IO, BS_UNCERTAIN, BS_RECOVERY };
enum { BS_READ=1u, BS_WRITE=2u };
struct bs_backend {
    void *context;
    uint64_t identity,sectors;
    bool writable,system_media;
    bool (*read)(void *,uint64_t,uint8_t[BS_SECTOR_BYTES]);
    bool (*write)(void *,uint64_t,const uint8_t[BS_SECTOR_BYTES]);
    bool (*flush)(void *);
};
struct bs_device {
    struct bs_backend backend;
    uint64_t generation;
    bool recovery;
};
struct bs_grant {
    uint64_t token,owner,generation,start,length;
    unsigned device,rights;
};
struct bs_service {
    struct bs_device devices[BS_DEVICES];
    struct bs_grant grants[BS_GRANTS];
    uint64_t serial;
    bool busy;
    uint8_t staging[BS_SECTOR_BYTES];
};
/* All management APIs belong to the trusted supervisor; owner is a verified,
 * lifetime-unique principal, NOT an untrusted caller-provided PID. */
enum bs_status bs_attach(struct bs_service *,const struct bs_backend *,uint64_t *generation);
enum bs_status bs_detach(struct bs_service *,uint64_t generation);
enum bs_status bs_grant_range(struct bs_service *,uint64_t generation,uint64_t owner,
                             uint64_t start,uint64_t length,unsigned rights,uint64_t *token);
enum bs_status bs_revoke(struct bs_service *,uint64_t owner,uint64_t token);
enum bs_status bs_revoke_owner(struct bs_service *,uint64_t owner);
/* Sector-relative operations. A read publishes only a complete sector.
 * A write includes its flush; failed write/flush is uncertain, no retry.
 * This is not a multi-sector transaction or file CRC validation. */
enum bs_status bs_read_sector(struct bs_service *,uint64_t owner,uint64_t token,
                              uint64_t sector,uint8_t[BS_SECTOR_BYTES]);
enum bs_status bs_write_sector(struct bs_service *,uint64_t owner,uint64_t token,
                               uint64_t sector,const uint8_t[BS_SECTOR_BYTES]);
/* Explicit opt-in for a backend that supports multiple pending writes.
 * ONLY unpublished, exclusively allocated data, never live metadata.
 * At most 64 sectors; last partial sector zero padded. Whole range
 * validated before I/O. Success means one completed flush; any write or
 * flush failure is uncertain for the entire batch, no partial success.
 * Existing bs_write_sector semantics are unchanged. */
enum bs_status bs_write_staged_batch(struct bs_service *,uint64_t owner,uint64_t token,
                                      uint64_t sector,const uint8_t *,size_t bytes);
#endif
