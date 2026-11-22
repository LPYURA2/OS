#ifndef STYLEOS_CACHE_EXTENT_804_H
#define STYLEOS_CACHE_EXTENT_804_H
#include <stdbool.h>
#include <stdint.h>
/* QEMU-only physical MMIO extent planning; no mapping, no hardware writes. */
struct cache804_span {uint64_t start,end,pages;};
bool cache804_plan(uint64_t start,uint64_t bytes,uint64_t max_bytes,struct cache804_span *out);
bool cache804_leaf_end(uint64_t cursor,uint64_t end,uint64_t leaf_bytes,uint64_t *stop);
#endif
