#include "styleos/paging.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

_Alignas(PG_PAGE) static uint64_t tables[PG_TABLE_PAGES][512];
static unsigned char saved[sizeof tables];

static struct pg_plan example(enum pg_arch arch)
{
    struct pg_plan p = {
        .arch = arch, .pa_bits = 40, .tables = tables,
        /* Deliberately unrelated to the host pointer: pg_build/lookup must
         * translate child table PAs through this supplied array relationship. */
        .tables_pa = UINT64_C(0x400a0000), .capacity = PG_TABLE_PAGES,
        .count = 2, .probe = PG_PROBE,
        .maps = {
            {UINT64_C(0x40080000), UINT64_C(0x40080000), 0x80000,
             PG_WRITE | PG_EXEC},
            {PG_ALIAS, UINT64_C(0x40090000), PG_PAGE, PG_WRITE}
        }
    };
    if (arch == PG_ARM) {
        p.maps[2] = (struct pg_mapping){0x9000000, 0x9000000,
                                      PG_PAGE, PG_WRITE | PG_DEVICE};
        p.count++;
    }
    return p;
}

static struct pg_plan single(enum pg_arch arch)
{
    struct pg_plan p = example(arch);
    p.count = 1;
    p.maps[0] = (struct pg_mapping){0x1000, 0x2000, PG_PAGE, PG_WRITE};
    return p;
}

static void refusal(struct pg_plan *p, enum pg_error expected, int untouched)
{
    memset(tables, 0xa5, sizeof tables);
    memcpy(saved, tables, sizeof tables);
    struct pg_result r = pg_build(p);
    assert(r.error == expected && r.root == 0 && r.used == 0);
    if (untouched) assert(!memcmp(saved, tables, sizeof tables));
}

static void assert_mapping(const struct pg_plan *p, uint64_t va,
                           uint64_t pa, unsigned flags)
{
    struct pg_lookup lookup = {0};
    assert(pg_lookup(p, va, &lookup) == PG_OK);
    assert(lookup.pa == pa && lookup.flags == flags);
}

static uint64_t *leaf(struct pg_plan *p, uint64_t va)
{
    uint64_t mask = ((UINT64_C(1) << p->pa_bits) - 1) & ~UINT64_C(4095);
    size_t table = 0;
    for (unsigned level = 0; level < 3; ++level) {
        uint64_t next = p->tables[table][(va >> (39u - 9u * level)) & 511u] & mask;
        assert(next >= p->tables_pa && next - p->tables_pa < p->capacity * PG_PAGE);
        table = (size_t)((next - p->tables_pa) / PG_PAGE);
    }
    return &p->tables[table][(va >> 12) & 511u];
}

static void successful_build(enum pg_arch arch)
{
    struct pg_plan p = example(arch);
    memset(tables, 0xa5, sizeof tables);
    struct pg_result r = pg_build(&p);
    assert(r.error == PG_OK && r.root == p.tables_pa);
    assert(r.root != (uint64_t)(uintptr_t)tables);
    assert(r.used >= 7 && r.used <= p.capacity);
    for (size_t n = r.used; n < p.capacity; ++n)
        for (size_t i = 0; i < 512; ++i) assert(tables[n][i] == 0);
    for (size_t i = 0; i < p.count; ++i)
        for (uint64_t offset = 0; offset < p.maps[i].bytes; offset += PG_PAGE) {
            assert_mapping(&p, p.maps[i].va + offset,
                           p.maps[i].pa + offset, p.maps[i].flags);
            assert_mapping(&p, p.maps[i].va + offset + PG_PAGE - 1,
                           p.maps[i].pa + offset + PG_PAGE - 1, p.maps[i].flags);
        }
    struct pg_lookup lookup = {123, 456};
    assert(pg_lookup(&p, PG_PROBE, &lookup) == PG_RANGE);
    assert(lookup.pa == 123 && lookup.flags == 456);
    assert(pg_lookup(&p, 0, &lookup) == PG_RANGE);
    assert(pg_lookup(&p, UINT64_C(1) << 47, &lookup) == PG_RANGE);
    assert(pg_lookup(&p, PG_ALIAS, 0) == PG_ARGUMENT);
    assert(pg_lookup(0, PG_ALIAS, &lookup) == PG_ARGUMENT);

    /* Rebuilding produces no dangling descendants from a previous plan. */
    p = single(arch);
    r = pg_build(&p);
    assert(r.error == PG_OK && r.used == 4);
    assert_mapping(&p, 0x1fff, 0x2fff, PG_WRITE);
    assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_RANGE);
}

static void input_refusals(enum pg_arch arch)
{
    struct pg_plan p = example(arch);
    refusal(0, PG_ARGUMENT, 1);
    p.arch = 0; refusal(&p, PG_ARGUMENT, 1);
    p = example(arch); p.pa_bits = 31; refusal(&p, PG_WIDTH, 1);
    p.pa_bits = arch == PG_X86 ? 53 : 49; refusal(&p, PG_WIDTH, 1);
    p = example(arch); p.tables = 0; refusal(&p, PG_ARGUMENT, 1);
    p = example(arch); p.tables_pa = 0; refusal(&p, PG_ARGUMENT, 1);
    p = example(arch); p.capacity = 0; refusal(&p, PG_CAPACITY, 1);
    p = example(arch); p.capacity = PG_TABLE_PAGES + 1; refusal(&p, PG_CAPACITY, 1);
    p = example(arch); p.tables_pa++; refusal(&p, PG_ALIGNMENT, 1);
    p = example(arch);
    p.tables = (uint64_t (*)[512])((uintptr_t)tables + 8);
    refusal(&p, PG_ALIGNMENT, 1);
    p = example(arch); p.tables_pa = UINT64_MAX & ~UINT64_C(4095);
    refusal(&p, PG_OVERFLOW, 1);
    p = example(arch);
    p.tables = (uint64_t (*)[512])(uintptr_t)(UINTPTR_MAX & ~(uintptr_t)4095);
    refusal(&p, PG_OVERFLOW, 1);
    p = example(arch); p.tables_pa = (UINT64_C(1) << 40) - PG_PAGE;
    refusal(&p, PG_WIDTH, 1);
    p = example(arch); p.count = 0; refusal(&p, PG_CAPACITY, 1);
    p = example(arch); p.count = PG_MAX_MAPS + 1; refusal(&p, PG_CAPACITY, 1);
    p = example(arch); p.probe++; refusal(&p, PG_ALIGNMENT, 1);
    p = example(arch); p.probe = UINT64_C(1) << 47; refusal(&p, PG_RANGE, 1);
    p = example(arch); p.probe = p.maps[0].va; refusal(&p, PG_PROBE_MAPPED, 1);
    p = example(arch); p.probe = p.maps[0].va + p.maps[0].bytes - PG_PAGE;
    refusal(&p, PG_PROBE_MAPPED, 1);
    p = example(arch); p.maps[0].bytes = 0; refusal(&p, PG_RANGE, 1);
    p = example(arch); p.maps[0].bytes--; refusal(&p, PG_ALIGNMENT, 1);
    p = example(arch); p.maps[0].va++; refusal(&p, PG_ALIGNMENT, 1);
    p = example(arch); p.maps[0].pa++; refusal(&p, PG_ALIGNMENT, 1);
    p = example(arch); p.maps[0].va = UINT64_MAX & ~UINT64_C(4095);
    refusal(&p, PG_OVERFLOW, 1);
    p = example(arch); p.maps[0].pa = UINT64_MAX & ~UINT64_C(4095);
    refusal(&p, PG_OVERFLOW, 1);
    p = example(arch); p.maps[0].va = (UINT64_C(1) << 47) - PG_PAGE;
    refusal(&p, PG_RANGE, 1);
    p = example(arch); p.maps[0].pa = (UINT64_C(1) << 40) - PG_PAGE;
    refusal(&p, PG_WIDTH, 1);
    p = example(arch); p.maps[0].flags |= 8; refusal(&p, PG_RIGHTS, 1);
    p = example(arch); p.maps[0].flags |= PG_DEVICE; refusal(&p, PG_RIGHTS, 1);
    p = example(arch); p.maps[1].va = p.maps[0].va;
    refusal(&p, PG_CONFLICT, 1);
    p = example(arch); p.maps[1].va = p.maps[0].va + p.maps[0].bytes - PG_PAGE;
    refusal(&p, PG_CONFLICT, 1);
    p = example(arch); p.maps[1].flags = 0; refusal(&p, PG_RIGHTS, 1);
    p = example(arch); p.maps[0].flags = PG_WRITE;
    p.maps[1].flags = PG_WRITE | PG_EXEC; refusal(&p, PG_RIGHTS, 1);
    p = example(arch); p.maps[1].flags = PG_WRITE | PG_DEVICE;
    refusal(&p, PG_RIGHTS, 1);
    /* Clearing the table buffer must never destroy the live input plan. */
    p = example(arch);
    memcpy(tables, &p, sizeof p);
    struct pg_plan *overlapping = (struct pg_plan *)(void *)tables;
    memcpy(saved, tables, sizeof tables);
    struct pg_result rejected = pg_build(overlapping);
    assert(rejected.error == PG_CONTEXT && !rejected.root && !rejected.used);
    assert(!memcmp(saved, tables, sizeof tables));
    if (arch == PG_X86) {
        p = single(arch); p.maps[0].flags = PG_WRITE | PG_DEVICE;
        refusal(&p, PG_RIGHTS, 1);
    }
}

static void edges_and_exhaustion(enum pg_arch arch)
{
    struct pg_plan p = single(arch);
    p.maps[0].va = (UINT64_C(1) << 47) - PG_PAGE;
    p.maps[0].pa = (UINT64_C(1) << 40) - PG_PAGE;
    struct pg_result r = pg_build(&p);
    assert(r.error == PG_OK);
    assert_mapping(&p, (UINT64_C(1) << 47) - 1,
                   (UINT64_C(1) << 40) - 1, PG_WRITE);
    p = single(arch);
    p.tables_pa = (UINT64_C(1) << 40) - PG_TABLE_BYTES;
    assert(pg_build(&p).error == PG_OK); /* Complete table extent ends at limit. */
    p = single(arch); p.capacity = 3;
    refusal(&p, PG_CAPACITY, 0); /* Partial scratch writes never publish a root. */
    p.capacity = 4;
    r = pg_build(&p);
    assert(r.error == PG_OK && r.used == 4);
    p = single(arch); p.capacity = 4; p.maps[0].bytes = PG_PAGE * 513u;
    refusal(&p, PG_CAPACITY, 0); /* Crosses a 2-MiB boundary, needs another L3. */
    p = single(arch); p.count = 2;
    p.maps[1] = (struct pg_mapping){0x2000, 0x3000, PG_PAGE, 0};
    assert(pg_build(&p).error == PG_OK); /* Adjacent VA and PA intervals. */
    assert_mapping(&p, 0x2000, 0x3000, 0);
}

static void damaged_descriptors(enum pg_arch arch)
{
    struct pg_plan p = example(arch);
    struct pg_lookup lookup = {123, 456};
    assert(pg_build(&p).error == PG_OK);
    uint64_t *entry = leaf(&p, PG_ALIAS), original = *entry;
    *entry = 0;
    assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_RANGE);
    assert(lookup.pa == 123 && lookup.flags == 456);
    *entry = original | 4u; /* x86 user; ARM changes MAIR slot to Device. */
    if (arch == PG_ARM) *entry = original | UINT64_C(16); /* unknown AttrIndx4 */
    assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_RIGHTS);
    *entry = original | (UINT64_C(1) << 40); /* Outside supported physical width. */
    assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_RIGHTS);
    if (arch == PG_ARM) {
        *entry = original & ~UINT64_C(1024); /* Missing Access Flag. */
        assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_RIGHTS);
        *entry = original & ~(UINT64_C(1) << 54); /* EL0 execute allowed. */
        assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_RIGHTS);
        *entry = original ^ UINT64_C(256); /* Wrong shareability. */
        assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_RIGHTS);
    } else {
        *entry = original | UINT64_C(96); /* CPU Accessed/Dirty updates are valid. */
        assert_mapping(&p, PG_ALIAS, p.maps[1].pa, PG_WRITE);
        tables[0][(PG_ALIAS >> 39) & 511u] |= UINT64_C(32);
        assert_mapping(&p, PG_ALIAS, p.maps[1].pa, PG_WRITE);
    }
    *entry = original;
    const size_t index = (size_t)((PG_ALIAS >> 39) & 511u);
    uint64_t parent = tables[0][index];
    tables[0][index] = parent | UINT64_C(128); /* Huge/block at intermediate level. */
    assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_RIGHTS);
    tables[0][index] = (p.tables_pa + p.capacity * PG_PAGE) | 3u;
    assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_CONTEXT);
    tables[0][index] = p.tables_pa | 3u; /* Loop back into the root. */
    assert(pg_lookup(&p, PG_ALIAS, &lookup) == PG_CONTEXT);
    tables[0][index] = parent;
    assert_mapping(&p, PG_ALIAS, p.maps[1].pa, PG_WRITE);
}

int main(void)
{
    for (enum pg_arch arch = PG_X86; arch <= PG_ARM; ++arch) {
        successful_build(arch);
        input_refusals(arch);
        edges_and_exhaustion(arch);
        damaged_descriptors(arch);
    }
    for (int error = PG_OK; error <= PG_PLATFORM; ++error)
        assert(strcmp(pg_error_name((enum pg_error)error), "unknown"));
    assert(!strcmp(pg_error_name((enum pg_error)999), "unknown"));
    puts("paging constructor/lookup host tests PASS (x86-64 + AArch64)");
    return 0;
}
