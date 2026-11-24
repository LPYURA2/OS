#ifndef STYLEOS_PHYSICAL_ARENA_844_H
#define STYLEOS_PHYSICAL_ARENA_844_H
#include <styleos/boot_memory.h>
#include <stdbool.h>
#define S844_PAGES 16u
#define S844_BYTES (S844_PAGES*BOOT_MEMORY_PAGE)
struct s844_lease { uint64_t pa,generation; uint32_t owner,pages; };
struct s844_arena {
 uint64_t base,next;
 struct s844_lease slots[S844_PAGES];
 bool ready;
};
/* Metadata must be zero-initialized before first init and kept outside arena.
 * Single CPU, kernel-only, serialized calls. The caller must own and map the
 * entire identity arena RW/NX/WB BEFORE init. No general RAM discovery or
 * mapping, no revocation of previously copied kernel pointers, no DMA claim. */
bool s844_init(struct s844_arena *,uint64_t base);
bool s844_acquire(struct s844_arena *,uint32_t owner,uint32_t pages,struct s844_lease *);
bool s844_release(struct s844_arena *,uint32_t owner,struct s844_lease);
bool s844_count(const struct s844_arena *,unsigned *);
bool s844_efi_proof(const struct boot_memory_evidence *,const struct boot_info *,
 struct boot_range,struct boot_range *,uint64_t *);
#endif
