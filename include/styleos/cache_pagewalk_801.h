#ifndef STYLEOS_CACHE_PAGEWALK_801_H
#define STYLEOS_CACHE_PAGEWALK_801_H
#include <stdint.h>
#include <stdbool.h>
/* Pure, read-only 4-level x86-64 walker. No CR3 writes or MMIO accesses.
 * An absent translation is evidence of absence at this sampled VA only.
 * No inverse scan or exhaustive alias proof is implied. */
typedef bool (*cache801_read_entry)(void *, uint64_t table_pa, uint16_t slot, uint64_t *entry);
struct cache801_translation {
    bool present;
    uint64_t leaf_entry;
    uint64_t physical;
    uint64_t page_size;
    uint8_t pat_index;
    uint8_t pat_type;
};
bool cache801_walk(uint64_t cr3, uint64_t va, uint64_t pat,
                   uint8_t physical_bits, cache801_read_entry read_entry,
                   void *context, struct cache801_translation *out);
#endif
