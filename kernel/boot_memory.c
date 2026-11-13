#include "styleos/boot_memory.h"

enum boot_memory_error bm_extent(uint64_t base, uint64_t size,
                                 struct boot_range *out)
{
    if (!size) return BM_RANGE;
    if (base > UINT64_MAX - size) return BM_OVERFLOW;
    *out = (struct boot_range){base, base + size};
    return BM_OK;
}

enum boot_memory_error bm_add(struct boot_memory_map *m, uint64_t base,
                              uint64_t size, int reserved)
{
    struct boot_range r;
    enum boot_memory_error error = bm_extent(base, size, &r);
    if (error) return error;
    struct boot_range *ranges = reserved ? m->reserved : m->ram;
    size_t *count = reserved ? &m->nr_reserved : &m->nr_ram;
    if (*count == BOOT_MEMORY_INPUT_MAX) return BM_LIMIT;
    ranges[(*count)++] = r;
    return BM_OK;
}

static void sort(struct boot_range *ranges, size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        struct boot_range next = ranges[i];
        size_t j = i;
        while (j && next.start < ranges[j - 1].start) {
            ranges[j] = ranges[j - 1];
            --j;
        }
        ranges[j] = next;
    }
}

int bm_ram_contains(const struct boot_memory_map *m, struct boot_range range)
{
    if (range.start >= range.end) return 0;
    for (size_t i = 0; i < m->nr_ram; ++i)
        if (range.start >= m->ram[i].start && range.end <= m->ram[i].end)
            return 1;
    return 0;
}

static int round_up(uint64_t address, uint64_t *out)
{
    const uint64_t mask = BOOT_MEMORY_PAGE - 1;
    uint64_t extra = (BOOT_MEMORY_PAGE - (address & mask)) & mask;
    if (address > UINT64_MAX - extra) return 0;
    *out = address + extra;
    return 1;
}

static enum boot_memory_error emit(struct boot_memory_map *m, uint64_t start,
                                    uint64_t end, size_t *count)
{
    if (start >= end) return BM_OK;
    if (*count && m->candidates[*count - 1].end == start) {
        m->candidates[*count - 1].end = end;
        return BM_OK;
    }
    if (*count == BOOT_MEMORY_RANGE_MAX) return BM_LIMIT;
    m->candidates[(*count)++] = (struct boot_range){start, end};
    return BM_OK;
}

enum boot_memory_error bm_normalize(struct boot_memory_map *m)
{
    size_t count = 0, reserved = 0, cursor = 0;
    m->nr_candidates = 0;
    sort(m->ram, m->nr_ram);
    sort(m->reserved, m->nr_reserved);
    for (size_t i = 1; i < m->nr_ram; ++i)
        if (m->ram[i].start < m->ram[i - 1].end) return BM_CONFLICT;

    /* Sorting raw starts also sorts their aligned starts. Coalesce the
     * overlapping/adjacent protected pages before subtracting them. */
    for (size_t i = 0; i < m->nr_reserved; ++i) {
        struct boot_range r = m->reserved[i];
        r.start &= ~(BOOT_MEMORY_PAGE - 1);
        if (!round_up(r.end, &r.end)) return BM_OVERFLOW;
        if (reserved && r.start <= m->reserved[reserved - 1].end) {
            if (r.end > m->reserved[reserved - 1].end)
                m->reserved[reserved - 1].end = r.end;
        } else {
            m->reserved[reserved++] = r;
        }
    }
    m->nr_reserved = reserved;
    for (size_t i = 0; i < m->nr_ram; ++i) {
        struct boot_range r = m->ram[i];
        if (!round_up(r.start, &r.start)) return BM_OVERFLOW;
        r.end &= ~(BOOT_MEMORY_PAGE - 1);
        if (r.start >= r.end) continue;
        while (cursor < reserved && m->reserved[cursor].end <= r.start) ++cursor;
        while (cursor < reserved && m->reserved[cursor].start < r.end) {
            struct boot_range cut = m->reserved[cursor];
            enum boot_memory_error error = emit(m, r.start, cut.start, &count);
            if (error) return error;
            r.start = cut.end;
            if (r.start >= r.end) break;
            ++cursor;
        }
        enum boot_memory_error error = emit(m, r.start, r.end, &count);
        if (error) return error;
    }
    if (!count) return BM_EMPTY;
    m->nr_candidates = count;
    return BM_OK;
}
