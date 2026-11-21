#include <styleos/user_copy_772.h>

enum uc772_status uc772_plan(uint64_t observed_cr3, uint64_t verified_cr3,
                             uint64_t page, uint64_t source, size_t bytes,
                             size_t *offset)
{
    if (!offset || !observed_cr3 || !verified_cr3 ||
        (observed_cr3 & (UC772_PAGE - 1u)) ||
        (verified_cr3 & (UC772_PAGE - 1u))) return UC772_BAD;
    if (observed_cr3 != verified_cr3) return UC772_ROOT;
    if ((page & (UC772_PAGE - 1u)) ||
        !bytes || bytes > UC772_MAX_COPY) return UC772_BAD;
    /* source must lie in this single verified page. No wrap-around and no
     * crossing into an untrusted second page, even by one byte. */
    if (source < page || source - page >= UC772_PAGE ||
        bytes > UC772_PAGE - (size_t)(source - page)) return UC772_RANGE;
    *offset = (size_t)(source - page);
    return UC772_OK;
}

enum uc772_status uc772_copy_page(void *output, size_t capacity,
                                  const void *backing, size_t offset,
                                  size_t count)
{
    if (!output || !backing || !count || count > UC772_MAX_COPY ||
        count > capacity || offset >= UC772_PAGE ||
        count > UC772_PAGE - offset) return UC772_BAD;
    const volatile unsigned char *src =
        (const volatile unsigned char *)backing + offset;
    unsigned char *dst = (unsigned char *)output;
    for (size_t i = 0; i < count; ++i) dst[i] = src[i];
    return UC772_OK;
}
