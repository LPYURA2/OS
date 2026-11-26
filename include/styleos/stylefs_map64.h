#ifndef STYLEOS_MAP64_H
#define STYLEOS_MAP64_H
#include <styleos/stylefs_extent64.h>

/* Candidate versioned file extent manifest, NOT a mounted filesystem.
 * Explicit LE encoding, never dump C structs. All buffers trusted, disjoint.
 * Caller owns immutable validated bytes while a view is used; caller also
 * verifies global allocator/snapshot ownership before allowing writes. */
enum { SFS64_MAP_HEADER=64, SFS64_MAP_RECORD=16 };
struct sfs64_run {uint64_t first,sectors;};
struct sfs64_file {
    uint64_t id,revision,bytes;
    uint32_t checksum;
};
struct sfs64_map {
    struct sfs64_file file;
    uint64_t capacity,data_start;
    uint32_t count;
    const uint8_t *records;
};
/* Exact manifest length; zero if count cannot be represented. */
size_t sfs64_map_length(size_t count);
bool sfs64_map_encode(uint8_t *,size_t,struct sfs64_file,uint64_t capacity,
                      uint64_t data_start,const struct sfs64_run *,size_t count);
bool sfs64_map_decode(const uint8_t *,size_t,uint64_t capacity,uint64_t data_start,
                      struct sfs64_map *);
/* Resolve one bounded data window; caller advances by n and repeats.
 * Runs are in logical file order, may be noncontiguous or out of physical
 * order. The mapped extent never contains data belonging to another run.
 * No raw disk I/O, no allocation, no metadata publication in this module. */
bool sfs64_map_chunk(const struct sfs64_map *,uint64_t offset,size_t wanted,
                     struct sfs_extent64 *,uint64_t *local,size_t *n);
#endif
