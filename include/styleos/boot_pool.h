#ifndef STYLEOS_BOOT_POOL_H
#define STYLEOS_BOOT_POOL_H
#include "styleos/boot_memory.h"

#define BOOT_POOL_PAGE 4096u
#define BOOT_POOL_PAGES 8u
#define BOOT_POOL_BYTES (BOOT_POOL_PAGE * BOOT_POOL_PAGES)
#define BOOT_POOL_METADATA_BYTES 4096u

/* Internal one-CPU pool of mapped kernel addresses in ONE image-owned array.
 * Not a physical-address translator, PMM, heap or userspace interface. */
enum boot_pool_error {
    BP_OK, BP_NOT_READY, BP_ALREADY_READY, BP_BAD_ARGUMENT, BP_RANGE,
    BP_OVERFLOW, BP_ALIGNMENT, BP_OVERLAP, BP_NOT_RESERVED, BP_EXHAUSTED,
    BP_OUTSIDE, BP_DOUBLE_FREE, BP_CONTEXT
};
struct boot_pool_result { void *page; enum boot_pool_error error; };

/* Pure range validation: no buffer/state writes, also used by host tests. */
enum boot_pool_error boot_pool_check_layout(uint64_t base, uint64_t bytes,
    uint64_t metadata_base, uint64_t metadata_bytes,
    const struct boot_memory_evidence *);
enum boot_pool_error boot_pool_admit(const struct boot_memory_evidence *);
struct boot_pool_result boot_pool_alloc(void);
enum boot_pool_error boot_pool_free(void *page);
struct boot_range boot_pool_storage_range(void);
struct boot_range boot_pool_metadata_range(void);
const char *boot_pool_error_name(enum boot_pool_error);

#ifdef STYLEOS_TEST_BOOTPOOL
void boot_pool_selftest(const struct boot_memory_evidence *);
#endif
#endif
