#include "styleos/arch.h"
#include "styleos/paging.h"
#include "styleos/boot_pool.h"
#ifdef STYLEOS_TEST_FRAME_GRANTS
#include "styleos/frame_grant_proof.h"
#endif

_Alignas(PG_PAGE) uint64_t styleos_paging_tables[PG_TABLE_PAGES][512];
_Alignas(PG_PAGE) union pg_control styleos_paging_control;
_Alignas(PG_PAGE) uint64_t styleos_paging_data[PG_DATA_BYTES / sizeof(uint64_t)] = {
    PG_INITIAL
};
_Static_assert(sizeof styleos_paging_tables == PG_TABLE_BYTES, "tables size");
_Static_assert(sizeof styleos_paging_control == PG_CONTROL_BYTES, "control size");
_Static_assert(sizeof styleos_paging_data == PG_DATA_BYTES, "data size");
_Static_assert(sizeof(struct pg_experiment) <= PG_CONTROL_BYTES, "control bound");

void paging_hex(uint64_t v)
{
    arch_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        arch_putc("0123456789abcdef"[(v >> shift) & 15u]);
}
void paging_range(const char *name, uint64_t start, uint64_t end)
{
    arch_puts("STYLEOS PAGING "); arch_puts(name); arch_putc(' ');
    paging_hex(start); arch_putc(' '); paging_hex(end); arch_putc('\n');
}
_Noreturn void paging_fail(const char *why)
{
    arch_puts("STYLEOS PAGING ERROR: "); arch_puts(why); arch_putc('\n');
    arch_halt();
}
static struct boot_range extent(const void *p, size_t size)
{
    struct boot_range r;
    if (bm_extent((uint64_t)(uintptr_t)p, size, &r)) paging_fail("extent");
    return r;
}
static void map_line(const char *kind, uint64_t va, uint64_t pa,
                     uint64_t bytes, unsigned flags)
{
    arch_puts("STYLEOS PAGING "); arch_puts(kind); arch_puts(" va=");
    paging_hex(va); arch_puts(" pa="); paging_hex(pa);
    if (bytes) { arch_puts(" bytes="); paging_hex(bytes); }
    arch_puts(" flags="); paging_hex(flags); arch_putc('\n');
}
static void verify(struct pg_experiment *s, int print)
{
    for (size_t i = 0; i < s->plan.count; ++i) {
        const struct pg_mapping *m = &s->plan.maps[i];
        for (uint64_t n = 0; n < m->bytes; n += PG_PAGE) {
            struct pg_lookup found;
            if (pg_lookup(&s->plan, m->va + n, &found) != PG_OK ||
                found.pa != m->pa + n || found.flags != m->flags)
                paging_fail("live page mapping");
            if (print) map_line("PTE", m->va + n, found.pa, 0, found.flags);
        }
    }
    struct pg_lookup absent;
    if (pg_lookup(&s->plan, PG_PROBE, &absent) != PG_RANGE)
        paging_fail("probe absence not proved");
}

void paging_experiment(const struct boot_memory_evidence *e)
{
    const struct pg_storage storage = {
        extent(styleos_paging_tables, sizeof styleos_paging_tables),
        extent(&styleos_paging_control, sizeof styleos_paging_control),
        extent(styleos_paging_data, sizeof styleos_paging_data)
    };
    enum pg_error error = pg_admit_layout(e, &storage,
        boot_pool_storage_range(), boot_pool_metadata_range());
    if (error) paging_fail(pg_error_name(error));
    /* First write to experimental working state follows full admission. */
    struct pg_experiment *s = &styleos_paging_control.state;
    *s = (struct pg_experiment){0};
    s->evidence = e;
    s->plan.arch = STYLEOS_EXPECT_ARCH == 1 ? PG_X86 : PG_ARM;
    s->plan.tables = styleos_paging_tables;
    s->plan.capacity = PG_TABLE_PAGES;
    s->plan.probe = PG_PROBE;
    /* Image closure deliberately preserves every image page RWX, including
     * pages shared by sections. This experiment makes no W^X claim. */
    uint64_t start = e->live.image.start;
    uint64_t end = e->live.image.end;
    if ((start & (PG_PAGE - 1u)) || end > UINT64_MAX - (PG_PAGE - 1u))
        paging_fail("image page extent");
    end = (end + PG_PAGE - 1u) & ~(uint64_t)(PG_PAGE - 1u);
    s->plan.maps[0] = (struct pg_mapping){start, start, end - start, PG_WRITE | PG_EXEC};
    s->plan.maps[1] = (struct pg_mapping){PG_ALIAS, storage.data.start, PG_DATA_BYTES, PG_WRITE};
    s->plan.count = 2;
    paging_range("TABLES", storage.tables.start, storage.tables.end);
    paging_range("CONTROL", storage.control.start, storage.control.end);
    paging_range("DATA", storage.data.start, storage.data.end);
    paging_range("WORKSPACE", e->workspace.start, e->workspace.end);
    error = arch_paging_prepare(s);
    if (error) paging_fail(pg_error_name(error));
    if (s->data_pa != storage.data.start || s->plan.tables_pa != storage.tables.start)
        paging_fail("image physical correspondence");
    s->plan.maps[1].pa = s->data_pa;
    for (size_t i = 0; i < s->plan.count; ++i) {
        struct pg_mapping *m = &s->plan.maps[i];
        map_line("MAP", m->va, m->pa, m->bytes, m->flags);
    }
    s->result = pg_build(&s->plan);
    if (s->result.error || !s->result.root)
        paging_fail(pg_error_name(s->result.error));
    verify(s, 0);
    arch_puts("STYLEOS PAGING ROOT pa="); paging_hex(s->result.root);
    arch_puts(" pages="); paging_hex(s->result.used); arch_putc('\n');
    error = arch_paging_activate(s);
    if (error) paging_fail(pg_error_name(error));
    verify(s, 1);
    volatile uint64_t *alias = (volatile uint64_t *)(uintptr_t)PG_ALIAS;
    volatile uint64_t *original = styleos_paging_data;
    uint64_t before = *alias;
    if (before != PG_INITIAL || *original != before) paging_fail("alias initial read");
    *alias = PG_WRITTEN;
    uint64_t after = *original;
    if (after != PG_WRITTEN || *alias != after) paging_fail("alias write readback");
    arch_puts("STYLEOS PAGING ALIAS va="); paging_hex(PG_ALIAS);
    arch_puts(" pa="); paging_hex(s->data_pa);
    arch_puts(" before="); paging_hex(before);
    arch_puts(" after="); paging_hex(after); arch_putc('\n');
#if defined(STYLEOS_TEST_PAGING_ROUNDTRIP) || defined(STYLEOS_TEST_FRAME_GRANTS)
    error = arch_paging_restore(s);
    if (error != PG_OK) paging_fail("guarded CR3 restore");
    verify(s, 0);
#ifdef STYLEOS_TEST_FRAME_GRANTS
    fg_experiment_run(s); /* only after inherited root has been restored */
#endif
    arch_puts("STYLEOS PAGING ROUNDTRIP PASS\n");
#elif defined(STYLEOS_TEST_PAGING_FAULT)
    extern void styleos_paging_fault(void);
    arch_puts("STYLEOS PAGING FAULT READY va="); paging_hex(PG_PROBE);
    arch_puts(" pc="); paging_hex((uint64_t)(uintptr_t)styleos_paging_fault);
    arch_putc('\n');
    arch_paging_fault(PG_PROBE);
    paging_fail("unmapped read returned");
#else
    arch_puts("STYLEOS PAGING PASS\n");
#endif
}
