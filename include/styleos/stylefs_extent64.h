#ifndef STYLEOS_EXTENT64_H
#define STYLEOS_EXTENT64_H
#include <stddef.h>
#include <styleos/block_service.h>

/* File-relative 64-bit byte offsets; sector numbers relative to the grant.
 * A trusted allocator owns the extent, and must prove that a staged write
 * targets only unreferenced storage. Not a user syscall or on-disk format.
 * No automatic metadata publication, allocation, retry or source removal.
 * The broker serializes requests and authenticates owner/token on every call. */
enum { SFS64_WINDOW_SECTORS=64, SFS64_WINDOW_BYTES=SFS64_WINDOW_SECTORS*BS_SECTOR_BYTES };
struct sfs_extent64 { uint64_t first,bytes; };
struct sfs_window64 { uint64_t sector; unsigned offset,bytes; };
uint64_t sfs64_sectors(uint64_t bytes);
bool sfs64_extent_fits(struct sfs_extent64,uint64_t grant_sectors);
/* Map a nonempty bounded byte window to its first sector fragment. */
bool sfs64_window(struct sfs_extent64,uint64_t grant_sectors,uint64_t offset,
                  uint64_t remaining,struct sfs_window64 *);
/* At most 64 sectors of I/O per request, including unaligned reads.
 * A read can expose provisional bytes; completed reports only successful
 * fragments. The future file transaction must verify integrity before publish.
 * Stage requires sector-aligned offset/length, except a partial final sector
 * at EOF (zero padded). Completed reports verified block-service writes,
 * not a committed file. A failed write may have changed media; do not retry
 * until recovery has been resolved. All pointers are trusted kernel spans. */
enum bs_status sfs64_read_window(struct bs_service *,uint64_t owner,uint64_t token,
    struct sfs_extent64,uint64_t offset,uint8_t *,size_t length,size_t *completed);
enum bs_status sfs64_stage_window(struct bs_service *,uint64_t owner,uint64_t token,
    struct sfs_extent64,uint64_t offset,const uint8_t *,size_t length,size_t *completed);
/* Explicit batch-capable backend only. Whole window flush confirmed or
 * completed=0; caller must still read back and verify before publishing. */
enum bs_status sfs64_stage_batch(struct bs_service *,uint64_t owner,uint64_t token,
    struct sfs_extent64,uint64_t offset,const uint8_t *,size_t length,size_t *completed);
#endif
