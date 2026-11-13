#include "styleos/boot_memory.h"

#define EFI_CONVENTIONAL_MEMORY 7u
/* For this diagnostic subset require WB capability. Known cache/XP bits
 * alone are harmless; unknown, runtime, special or protected bits are not. */
#define EFI_ATTR_WB UINT64_C(8)
#define EFI_ATTR_ALLOWED (UINT64_C(0x1f) | UINT64_C(0x4000))

static uint64_t le(const uint8_t *p, unsigned n)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < n; ++i) value |= (uint64_t)p[i] << (i * 8);
    return value;
}

static enum boot_memory_error descriptor(const uint8_t *p, struct boot_range *r)
{
    uint64_t base = le(p + 8, 8), pages = le(p + 24, 8);
    if (base & (BOOT_MEMORY_PAGE - 1)) return BM_UEFI_FORMAT;
    if (!pages) return BM_UEFI_FORMAT;
    if (pages > UINT64_MAX / BOOT_MEMORY_PAGE) return BM_OVERFLOW;
    return bm_extent(base, pages * BOOT_MEMORY_PAGE, r);
}

enum boot_memory_error bm_uefi_parse(struct boot_memory_map *m, const void *data,
                                     size_t size, size_t stride, uint32_t version)
{
    if (!data || version != 1 || stride < 40 || stride > 4096 ||
        (stride & 7) || !size || size % stride) return BM_UEFI_FORMAT;
    if (size / stride > BOOT_MEMORY_INPUT_MAX) return BM_LIMIT;
    const uint8_t *map = data;
    for (size_t at = 0; at < size; at += stride) {
        const uint8_t *p = map + at;
        struct boot_range r;
        enum boot_memory_error error = descriptor(p, &r);
        if (error) return error;
        for (size_t prev = 0; prev < at; prev += stride) {
            struct boot_range other;
            error = descriptor(map + prev, &other);
            if (error) return error;
            if (r.start < other.end && other.start < r.end) return BM_CONFLICT;
        }
        uint64_t attr = le(p + 32, 8);
        int usable = le(p, 4) == EFI_CONVENTIONAL_MEMORY &&
                     (attr & EFI_ATTR_WB) && !(attr & ~EFI_ATTR_ALLOWED);
        error = bm_add(m, r.start, r.end - r.start, !usable);
        if (error) return error;
    }
    return BM_OK;
}
