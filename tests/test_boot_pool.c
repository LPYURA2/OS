#include "styleos/boot_pool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern unsigned char styleos_bootpool_storage[BOOT_POOL_BYTES];

struct fixture {
    struct boot_memory_map map;
    struct boot_memory_evidence evidence;
};
static struct fixture fixture;
static unsigned char host_stack[65536];
static unsigned char saved_metadata[BOOT_POOL_METADATA_BYTES];

static uint64_t address(const void *p)
{
    return (uint64_t)(uintptr_t)p;
}

static struct boot_range object(const void *p, size_t n)
{
    return (struct boot_range){address(p), address(p) + n};
}

static uint64_t page_up(uint64_t n)
{
    assert(n <= UINT64_MAX - (BOOT_POOL_PAGE - 1u));
    return (n + BOOT_POOL_PAGE - 1u) & ~(uint64_t)(BOOT_POOL_PAGE - 1u);
}

/* Only proof/map are read here. Pool, metadata, stack and source may be numeric
 * ranges: check_layout must never access memory merely because it is named. */
static struct boot_memory_evidence *synthetic(void)
{
    memset(&fixture, 0, sizeof fixture);
    struct boot_memory_evidence *e = &fixture.evidence;
    e->workspace = object(&fixture, sizeof fixture);
    e->map = &fixture.map;
    e->live.image = (struct boot_range){0x10000,
        page_up(e->workspace.end + 0x200000)};
    e->live.stack = (struct boot_range){0x30000, 0x40000};
    e->live.handoff = (struct boot_range){0x30100, 0x30160};
    e->live.source = (struct boot_range){0x50000, 0x51000};
    fixture.map.nr_reserved = 1;
    fixture.map.reserved[0] = e->live.image;
    /* No allocator may inspect or trust these diagnostic-only entries. */
    fixture.map.nr_candidates = SIZE_MAX;
    fixture.map.candidates[0] = (struct boot_range){0, UINT64_MAX};
    return e;
}

static enum boot_pool_error layout(const struct boot_memory_evidence *e)
{
    return boot_pool_check_layout(0x10000, BOOT_POOL_BYTES,
                                  0x20000, BOOT_POOL_METADATA_BYTES, e);
}

static void pure_ranges(void)
{
    struct boot_memory_evidence *e = synthetic();
    assert(layout(e) == BP_OK);
    assert(layout(0) == BP_BAD_ARGUMENT);
    assert(boot_pool_check_layout(0x10000, 0, 0x20000,
        BOOT_POOL_METADATA_BYTES, e) == BP_RANGE);
    assert(boot_pool_check_layout(0x10000, BOOT_POOL_BYTES - 1, 0x20000,
        BOOT_POOL_METADATA_BYTES, e) == BP_RANGE);
    assert(boot_pool_check_layout(0x10000, BOOT_POOL_BYTES, 0x20000,
        BOOT_POOL_METADATA_BYTES + 1, e) == BP_RANGE);
    assert(boot_pool_check_layout(UINT64_MAX & ~UINT64_C(4095),
        BOOT_POOL_BYTES, 0x20000, BOOT_POOL_METADATA_BYTES, e) == BP_OVERFLOW);
    assert(boot_pool_check_layout(0x10000, BOOT_POOL_BYTES,
        UINT64_MAX & ~UINT64_C(4095), BOOT_POOL_METADATA_BYTES, e) == BP_OVERFLOW);
    assert(boot_pool_check_layout(0x10001, BOOT_POOL_BYTES, 0x20000,
        BOOT_POOL_METADATA_BYTES, e) == BP_ALIGNMENT);
    assert(boot_pool_check_layout(0x10000, BOOT_POOL_BYTES, 0x20001,
        BOOT_POOL_METADATA_BYTES, e) == BP_ALIGNMENT);
    assert(boot_pool_check_layout(0xf000, BOOT_POOL_BYTES, 0x20000,
        BOOT_POOL_METADATA_BYTES, e) == BP_RANGE);

    /* Exact half-open ends are accepted; one whole page beyond is rejected.
     * An end one byte short must fail even though the start is inside. */
    uint64_t end = e->live.image.end;
    assert(boot_pool_check_layout(end - BOOT_POOL_BYTES, BOOT_POOL_BYTES,
        0x20000, BOOT_POOL_METADATA_BYTES, e) == BP_OK);
    assert(boot_pool_check_layout(end - BOOT_POOL_BYTES + BOOT_POOL_PAGE,
        BOOT_POOL_BYTES, 0x20000, BOOT_POOL_METADATA_BYTES, e) == BP_RANGE);
    e->live.image.end = end - 1;
    assert(boot_pool_check_layout(end - BOOT_POOL_BYTES, BOOT_POOL_BYTES,
        0x20000, BOOT_POOL_METADATA_BYTES, e) == BP_RANGE);
    e = synthetic();
    end = e->live.image.end;
    assert(boot_pool_check_layout(0x10000, BOOT_POOL_BYTES,
        end - BOOT_POOL_METADATA_BYTES, BOOT_POOL_METADATA_BYTES, e) == BP_OK);
    e->live.image.end = end - 1;
    assert(boot_pool_check_layout(0x10000, BOOT_POOL_BYTES,
        end - BOOT_POOL_METADATA_BYTES, BOOT_POOL_METADATA_BYTES, e) == BP_RANGE);

    e = synthetic();
    fixture.map.nr_reserved = 0;
    assert(layout(e) == BP_NOT_RESERVED);
    fixture.map.nr_reserved = 1;
    fixture.map.reserved[0] = (struct boot_range){0x10000, 0x21000};
    assert(layout(e) == BP_NOT_RESERVED); /* Merely reserving the pool is insufficient. */
    fixture.map.reserved[0] = e->live.image;
    fixture.map.reserved[0].start++;
    assert(layout(e) == BP_NOT_RESERVED);
    fixture.map.reserved[0] = e->live.image;
    fixture.map.reserved[0].end--;
    assert(layout(e) == BP_NOT_RESERVED);
    fixture.map.nr_reserved = BOOT_MEMORY_INPUT_MAX + 1u;
    assert(layout(e) == BP_CONTEXT);
    fixture.map.nr_reserved = SIZE_MAX;
    assert(layout(e) == BP_CONTEXT);
    e = synthetic();
    fixture.map.reserved[0].end = fixture.map.reserved[0].start;
    assert(layout(e) == BP_CONTEXT);
}

static void exclusions_and_context(void)
{
    struct boot_memory_evidence *e = synthetic();
    assert(boot_pool_check_layout(0x10000, BOOT_POOL_BYTES, 0x17000,
        BOOT_POOL_METADATA_BYTES, e) == BP_OVERLAP);
    assert(boot_pool_check_layout(0x10000, BOOT_POOL_BYTES, 0x18000,
        BOOT_POOL_METADATA_BYTES, e) == BP_OK); /* Adjacent is disjoint. */

    struct boot_range *excluded[] = {
        &e->live.stack, &e->live.handoff, &e->live.source, &e->workspace
    };
    for (size_t i = 0; i < 3; ++i) {
        e = synthetic();
        *excluded[i] = (struct boot_range){0x17fff, 0x18000};
        assert(layout(e) == BP_OVERLAP);
        e = synthetic();
        *excluded[i] = (struct boot_range){0x20000, 0x20001};
        assert(layout(e) == BP_OVERLAP);
        e = synthetic();
        *excluded[i] = (struct boot_range){0x18000, 0x19000};
        assert(layout(e) == BP_OK);
    }
    e = synthetic();
    uint64_t workspace_page = e->workspace.start & ~UINT64_C(4095);
    assert(boot_pool_check_layout(workspace_page, BOOT_POOL_BYTES, 0x20000,
        BOOT_POOL_METADATA_BYTES, e) == BP_OVERLAP);
    assert(boot_pool_check_layout(0x10000, BOOT_POOL_BYTES, workspace_page,
        BOOT_POOL_METADATA_BYTES, e) == BP_OVERLAP);

    e = synthetic(); e->live.image.end = e->live.image.start;
    assert(layout(e) == BP_CONTEXT);
    e = synthetic(); e->live.stack.start = e->live.stack.end + 1;
    assert(layout(e) == BP_CONTEXT);
    e = synthetic(); e->live.handoff.end = e->live.image.end + 1;
    assert(layout(e) == BP_CONTEXT);
    e = synthetic(); e->live.source.end = e->live.source.start;
    assert(layout(e) == BP_CONTEXT);
    e = synthetic(); e->workspace.end = e->live.image.end + 1;
    assert(layout(e) == BP_CONTEXT);
    e = synthetic(); e->workspace.start = address(e);
    assert(layout(e) == BP_CONTEXT); /* Entire map must fit, not only evidence. */
    e = synthetic(); e->workspace.end = address(e) + sizeof *e - 1;
    assert(layout(e) == BP_CONTEXT); /* Entire evidence must fit too. */
    e = synthetic(); e->map = 0;
    assert(layout(e) == BP_CONTEXT);
    e = synthetic();
    assert(layout((const struct boot_memory_evidence *)(uintptr_t)
                  (address(e) + 1)) == BP_CONTEXT);
    assert(layout((const struct boot_memory_evidence *)(uintptr_t)
                  (UINT64_MAX & ~UINT64_C(7))) == BP_OVERFLOW);
    e = synthetic();
    e->map = (const struct boot_memory_map *)(uintptr_t)(UINT64_MAX - 7u);
    assert(layout(e) == BP_OVERFLOW); /* Rejected before reading the pointer. */
    e = synthetic();
    e->map = (const struct boot_memory_map *)(uintptr_t)(e->workspace.start + 1);
    assert(layout(e) == BP_CONTEXT);
    e = synthetic();
    e->map = (const struct boot_memory_map *)(uintptr_t)(e->live.image.end - 16);
    assert(layout(e) == BP_CONTEXT);
}

static struct boot_memory_evidence *real_layout(void)
{
    memset(&fixture, 0, sizeof fixture);
    struct boot_memory_evidence *e = &fixture.evidence;
    struct boot_range pool = boot_pool_storage_range();
    struct boot_range metadata = boot_pool_metadata_range();
    e->workspace = object(&fixture, sizeof fixture);
    e->map = &fixture.map;
    e->live.stack = object(host_stack, sizeof host_stack);
    e->live.handoff = (struct boot_range){e->live.stack.start + 128,
                                         e->live.stack.start + 224};
    e->live.source = (struct boot_range){0x1000, 0x2000};
    e->live.image = pool;
    const struct boot_range ranges[] = {metadata, e->workspace, e->live.stack};
    for (size_t i = 0; i < sizeof ranges / sizeof ranges[0]; ++i) {
        if (ranges[i].start < e->live.image.start) e->live.image.start = ranges[i].start;
        if (ranges[i].end > e->live.image.end) e->live.image.end = ranges[i].end;
    }
    e->live.image.start &= ~UINT64_C(4095);
    e->live.image.end = page_up(e->live.image.end);
    fixture.map.nr_reserved = 1;
    fixture.map.reserved[0] = e->live.image;
    return e;
}

static void bytes_are(const void *page, size_t bytes, unsigned char value)
{
    const unsigned char *p = page;
    for (size_t i = 0; i < bytes; ++i) assert(p[i] == value);
}

static const void *metadata_bytes(void)
{
    /* Round trip of the known global object's mapped address, not a physical
     * address translation or a read of a fabricated numeric test range. */
    return (const void *)(uintptr_t)boot_pool_metadata_range().start;
}

static void unchanged_before_admission(void)
{
    memset(styleos_bootpool_storage, 0xa5, BOOT_POOL_BYTES);
    memcpy(saved_metadata, metadata_bytes(), sizeof saved_metadata);
    struct boot_pool_result r = boot_pool_alloc();
    assert(!r.page && r.error == BP_NOT_READY);
    assert(boot_pool_free(styleos_bootpool_storage) == BP_NOT_READY);
    assert(boot_pool_admit(0) == BP_BAD_ARGUMENT);
    struct boot_memory_evidence *e = real_layout();
    fixture.map.nr_reserved = 0;
    assert(boot_pool_admit(e) == BP_NOT_RESERVED);
    e = real_layout(); e->live.image.end = boot_pool_storage_range().end - 1;
    assert(boot_pool_admit(e) != BP_OK);
    e = real_layout(); e->live.source = boot_pool_storage_range();
    assert(boot_pool_admit(e) == BP_OVERLAP);
    bytes_are(styleos_bootpool_storage, BOOT_POOL_BYTES, 0xa5);
    assert(!memcmp(saved_metadata, metadata_bytes(), sizeof saved_metadata));
    r = boot_pool_alloc();
    assert(!r.page && r.error == BP_NOT_READY);
}

static void allocator(void)
{
    struct boot_memory_evidence *e = real_layout();
    struct boot_range pool = boot_pool_storage_range();
    struct boot_range metadata = boot_pool_metadata_range();
    assert(pool.end - pool.start == BOOT_POOL_BYTES);
    assert(metadata.end - metadata.start == BOOT_POOL_METADATA_BYTES);
    assert(!(pool.start % BOOT_POOL_PAGE) && !(metadata.start % BOOT_POOL_PAGE));
    assert(boot_pool_admit(e) == BP_OK);
    memcpy(saved_metadata, metadata_bytes(), sizeof saved_metadata);
    assert(boot_pool_admit(e) == BP_ALREADY_READY);
    assert(boot_pool_admit(0) == BP_ALREADY_READY);
    assert(!memcmp(saved_metadata, metadata_bytes(), sizeof saved_metadata));
    bytes_are(styleos_bootpool_storage, BOOT_POOL_BYTES, 0xa5); /* Admission does not wipe. */

    unsigned char *pages[BOOT_POOL_PAGES];
    for (unsigned i = 0; i < BOOT_POOL_PAGES; ++i) {
        struct boot_pool_result r = boot_pool_alloc();
        assert(r.error == BP_OK && r.page);
        pages[i] = r.page;
        assert(!(address(r.page) % BOOT_POOL_PAGE));
        assert(address(r.page) >= pool.start &&
               address(r.page) + BOOT_POOL_PAGE <= pool.end);
        for (unsigned j = 0; j < i; ++j) assert(pages[i] != pages[j]);
        bytes_are(pages[i], BOOT_POOL_PAGE, 0);
        memset(pages[i], i + 1u, BOOT_POOL_PAGE);
    }
    memcpy(saved_metadata, metadata_bytes(), sizeof saved_metadata);
    struct boot_pool_result r = boot_pool_alloc();
    assert(!r.page && r.error == BP_EXHAUSTED);
    assert(boot_pool_free(0) == BP_OUTSIDE);
    assert(boot_pool_free((void *)(uintptr_t)(pool.start - 1)) == BP_OUTSIDE);
    assert(boot_pool_free((void *)(uintptr_t)pool.end) == BP_OUTSIDE);
    assert(boot_pool_free((void *)(uintptr_t)metadata.start) == BP_OUTSIDE);
    assert(boot_pool_free(pages[0] + 1) == BP_ALIGNMENT);
    assert(boot_pool_free(pages[7] + BOOT_POOL_PAGE - 1) == BP_ALIGNMENT);
    assert(!memcmp(saved_metadata, metadata_bytes(), sizeof saved_metadata));
    for (unsigned i = 0; i < BOOT_POOL_PAGES; ++i)
        bytes_are(pages[i], BOOT_POOL_PAGE, (unsigned char)(i + 1));
    r = boot_pool_alloc();
    assert(!r.page && r.error == BP_EXHAUSTED);

    assert(boot_pool_free(pages[3]) == BP_OK);
    memcpy(saved_metadata, metadata_bytes(), sizeof saved_metadata);
    assert(boot_pool_free(pages[3]) == BP_DOUBLE_FREE);
    assert(boot_pool_free(pages[3] + 1) == BP_ALIGNMENT);
    assert(!memcmp(saved_metadata, metadata_bytes(), sizeof saved_metadata));
    bytes_are(pages[3], BOOT_POOL_PAGE, 4); /* Refusal and free do not touch page bytes. */
    r = boot_pool_alloc();
    assert(r.error == BP_OK && r.page == pages[3]);
    bytes_are(r.page, BOOT_POOL_PAGE, 0);
    for (unsigned i = 0; i < BOOT_POOL_PAGES; ++i)
        if (i != 3) bytes_are(pages[i], BOOT_POOL_PAGE, (unsigned char)(i + 1));
    r = boot_pool_alloc();
    assert(!r.page && r.error == BP_EXHAUSTED);

    for (unsigned i = 0; i < BOOT_POOL_PAGES; ++i) assert(boot_pool_free(pages[i]) == BP_OK);
    for (unsigned i = 0; i < BOOT_POOL_PAGES; ++i) {
        r = boot_pool_alloc();
        assert(r.error == BP_OK && r.page == pages[i]);
        bytes_are(r.page, BOOT_POOL_PAGE, 0);
    }
    for (unsigned i = 0; i < BOOT_POOL_PAGES; ++i) assert(boot_pool_free(pages[i]) == BP_OK);
    for (unsigned i = 0; i < BOOT_POOL_PAGES; ++i) assert(boot_pool_free(pages[i]) == BP_DOUBLE_FREE);
}

int main(void)
{
    pure_ranges();
    exclusions_and_context();
    unchanged_before_admission();
    allocator();
    assert(!strcmp(boot_pool_error_name(BP_EXHAUSTED), "exhausted"));
    assert(!strcmp(boot_pool_error_name((enum boot_pool_error)999), "unknown"));
    puts("Boot pool: admission, full ranges, exclusions, refusals, allocation and reuse PASS");
    return 0;
}
