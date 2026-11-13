#ifndef STYLEOS_BOOT_MEMORY_H
#define STYLEOS_BOOT_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "styleos/boot_info.h"

/* Diagnostic only. No address in this map has been granted to an allocator. */
#define BOOT_MEMORY_PAGE UINT64_C(4096)
#define BOOT_MEMORY_INPUT_MAX 256u
#define BOOT_MEMORY_RANGE_MAX 256u
#define BOOT_DTB_MAX (16u * 1024u * 1024u)

struct boot_range { uint64_t start, end; };
struct boot_memory_map {
    struct boot_range ram[BOOT_MEMORY_INPUT_MAX];
    struct boot_range reserved[BOOT_MEMORY_INPUT_MAX];
    struct boot_range candidates[BOOT_MEMORY_RANGE_MAX];
    size_t nr_ram, nr_reserved, nr_candidates;
};

enum boot_memory_error {
    BM_OK, BM_RANGE, BM_OVERFLOW, BM_LIMIT, BM_CONFLICT, BM_EMPTY,
    BM_UEFI_FORMAT, BM_FDT_FORMAT, BM_FDT_UNSUPPORTED, BM_METADATA
};

struct boot_memory_live {
    struct boot_range image, stack, handoff, source;
};

/* Internal read-only evidence after successful diagnostic normalization. The
 * workspace covers the map and this snapshot, both reserved in the image.
 * This does not transfer ownership of any candidate range. */
struct boot_memory_evidence {
    struct boot_memory_live live;
    struct boot_range workspace;
    const struct boot_memory_map *map;
};

enum boot_memory_error bm_extent(uint64_t base, uint64_t size,
                                 struct boot_range *out);
enum boot_memory_error bm_add(struct boot_memory_map *, uint64_t base,
                              uint64_t size, int reserved);
enum boot_memory_error bm_normalize(struct boot_memory_map *);
enum boot_memory_error bm_uefi_parse(struct boot_memory_map *, const void *,
                                     size_t size, size_t stride, uint32_t version);
enum boot_memory_error bm_fdt_parse(struct boot_memory_map *, const void *,
                                    size_t available, uint32_t *total_size);
int bm_ram_contains(const struct boot_memory_map *, struct boot_range);

/* The platform collector supplies validated ranges and the exact live extents.
 * The common kernel adds all live extents before normalizing. */
enum boot_memory_error boot_memory_collect(const struct boot_info *,
                                            struct boot_memory_map *,
                                            struct boot_memory_live *);
const struct boot_memory_evidence *boot_memory_diagnose(const struct boot_info *);

#endif
