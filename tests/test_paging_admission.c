#include "styleos/paging.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    struct boot_memory_map map;
    struct boot_memory_evidence evidence;
} fixture;
static const struct boot_range pool = {0x10000, 0x18000};
static const struct boot_range metadata = {0x18000, 0x19000};
static struct pg_storage fresh(void)
{
    memset(&fixture, 0, sizeof fixture);
    struct boot_memory_evidence *e = &fixture.evidence;
    e->workspace = (struct boot_range){(uintptr_t)&fixture,
                                      (uintptr_t)&fixture + sizeof fixture};
    e->map = &fixture.map;
    e->live.image = (struct boot_range){0x10000,
        (e->workspace.end + 4095) & ~UINT64_C(4095)};
    e->live.stack = (struct boot_range){0x60000, 0x70000};
    e->live.handoff = (struct boot_range){0x60000, 0x60060};
    e->live.source = (struct boot_range){0x90000, 0x91000};
    fixture.map.nr_reserved = 1;
    fixture.map.reserved[0] = e->live.image;
    fixture.map.nr_candidates = SIZE_MAX; /* Must never be consulted. */
    return (struct pg_storage){{0x30000,0x50000},
                              {0x50000,0x51000}, {0x51000,0x52000}};
}
static enum pg_error check(struct pg_storage *s)
{
    return pg_admit_layout(&fixture.evidence, s, pool, metadata);
}
int main(void)
{
    struct pg_storage s = fresh();
    assert(check(&s) == PG_OK);
    assert(pg_admit_layout(0, &s, pool, metadata) == PG_ARGUMENT);
    assert(check(0) == PG_ARGUMENT);
    s.tables.end--; assert(check(&s) == PG_RANGE);
    s = fresh(); s.tables.start++; s.tables.end++; assert(check(&s) == PG_ALIGNMENT);
    s = fresh(); s.data.end = s.data.start; assert(check(&s) == PG_RANGE);
    s = fresh(); s.tables = (struct boot_range){UINT64_MAX-0x1ffff,0};
    assert(check(&s) == PG_RANGE);
    s = fresh(); s.control = s.data; assert(check(&s) == PG_CONFLICT);
    s = fresh(); s.control = metadata; assert(check(&s) == PG_CONFLICT);
    s = fresh(); s.data = (struct boot_range){0x10000,0x11000};
    assert(check(&s) == PG_CONFLICT);
    s = fresh(); fixture.evidence.live.source = s.data; assert(check(&s) == PG_CONFLICT);
    s = fresh(); fixture.evidence.live.stack = s.tables; assert(check(&s) == PG_CONFLICT);
    s = fresh(); fixture.evidence.live.handoff = s.data; assert(check(&s) == PG_CONFLICT);
    s = fresh(); s.data.start = fixture.evidence.workspace.start & ~UINT64_C(4095);
    s.data.end = s.data.start + 4096; assert(check(&s) == PG_CONFLICT);
    s = fresh(); s.data.end = fixture.evidence.live.image.end + 4096;
    s.data.start = s.data.end - 4096; assert(check(&s) == PG_RANGE);
    s = fresh(); fixture.map.nr_reserved = 0; assert(check(&s) == PG_CONTEXT);
    s = fresh(); fixture.map.reserved[0].end--; assert(check(&s) == PG_CONTEXT);
    s = fresh(); fixture.evidence.map = 0; assert(check(&s) == PG_CONTEXT);
    s = fresh();
    unsigned char saved[sizeof fixture]; memcpy(saved, &fixture, sizeof fixture);
    s.control = s.data; assert(check(&s) == PG_CONFLICT);
    assert(!memcmp(saved, &fixture, sizeof fixture));
    puts("paging admission host tests PASS");
    return 0;
}
