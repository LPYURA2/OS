#include <styleos/desktop_vm_owner.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* In the host this is a MOCK image and a MOCK old-root witness. Host success
 * proves admission logic only; it is NOT guest physical-memory ownership. */
static _Alignas(DVM_PAGE_SIZE) struct {
    struct dvm_storage tables;
    unsigned char user[DVM_USER_SLOTS][DVM_USER_PAGES][DVM_PAGE_SIZE];
} image_arena;
_Static_assert(sizeof(image_arena) == DVO_OWNED_BYTES, "no arena padding");
static struct boot_memory_map memory;
static struct boot_memory_evidence evidence;
static struct dvm_plan plan;
static struct dvm_output output;
static struct dvo_exclusions excluded;
static unsigned attempts;
static unsigned fail_at;

static uint64_t base(void) { return (uint64_t)(uintptr_t)&image_arena; }
static bool fake_identity(void *ctx, uint64_t va, uint64_t pa)
{
    const uint64_t *expected = (const uint64_t *)ctx;
    const unsigned at = attempts++;
    return va == pa && va == *expected + (uint64_t)at * DVM_PAGE_SIZE &&
           at < DVO_OWNED_PAGES && at != fail_at;
}
static void setup(void)
{
    memset(&image_arena, 0, sizeof(image_arena));
    memset(&memory, 0, sizeof(memory));
    memset(&evidence, 0, sizeof(evidence));
    memset(&plan, 0, sizeof(plan));
    memset(&excluded, 0, sizeof(excluded));
    memset(&output, 0, sizeof(output));
    attempts = 0; fail_at = DVO_OWNED_PAGES;
    const uint64_t p = base();
    assert(p && !(p & (DVM_PAGE_SIZE - 1u)) && p < (UINT64_C(1) << 47));
    assert(p >= DVM_PAGE_SIZE * 4u && p <= UINT64_MAX - DVO_OWNED_BYTES - DVM_PAGE_SIZE);
    evidence.live.image = (struct boot_range){p - DVM_PAGE_SIZE, p + DVO_OWNED_BYTES + DVM_PAGE_SIZE};
    evidence.live.stack = (struct boot_range){p - DVM_PAGE_SIZE, p};
    evidence.live.handoff = (struct boot_range){p + DVO_OWNED_BYTES, p + DVO_OWNED_BYTES + 16u};
    evidence.live.source = (struct boot_range){0x80000000u, 0x80001000u};
    evidence.workspace = (struct boot_range){0x80002000u, 0x80003000u};
    memory.nr_reserved = 1;
    memory.reserved[0] = evidence.live.image;
    evidence.map = &memory;
    plan.physical_table_base = p;
    plan.kernel_count = 2;
    plan.kernel[0] = (struct dvm_region){0x200000, 0x200000, 2, DVM_EXEC};
    plan.kernel[1] = (struct dvm_region){0x202000, 0x202000, 2, DVM_WRITE};
    for (unsigned slot = 0; slot < DVM_USER_SLOTS; ++slot) {
        const uint64_t u = DVM_USER_BEGIN + slot * 0x40000u;
        for (unsigned j = 0; j < DVM_USER_PAGES; ++j) {
            const unsigned n = DVO_TABLE_PAGES + slot * DVM_USER_PAGES + j;
            plan.user[slot][j] = (struct dvm_region){
                u + j * 0x10000u, p + (uint64_t)n * DVM_PAGE_SIZE, 1,
                j == 0 ? DVM_EXEC : DVM_WRITE};
        }
    }
}
static enum dvo_status go(void)
{
    const uint64_t b = base();
    return dvo_build_owned(&evidence, (struct dvo_arena){b, DVO_OWNED_BYTES},
                            excluded, &plan, &image_arena.tables, &output,
                            fake_identity, (void *)&b);
}
static void rejected(enum dvo_status code, unsigned witness_count)
{
    attempts = 0;
    memset(&image_arena.tables, 0xa5, sizeof(image_arena.tables));
    memset(&output, 0xa5, sizeof(output));
    const enum dvo_status result = go();
    if (result != code)
        fprintf(stderr,"expected %d, observed %d\n", (int)code, (int)result);
    assert(result == code && attempts == witness_count);
    for (unsigned i = 0; i < DVM_ROOTS; ++i) {
        assert(output.root[i] == 0 && output.used[i] == 0);
        for (unsigned t = 0; t < DVM_TABLES_PER_ROOT; ++t)
            assert(image_arena.tables.table[i][t][0] ==
                   (code == DVO_BUILD ? 0 : UINT64_C(0xa5a5a5a5a5a5a5a5)));
    }
}
int main(void)
{
    setup();
    assert(go() == DVO_OK && attempts == DVO_OWNED_PAGES);
    assert(output.root[0] == base());
    assert(output.root[1] == base() + DVM_TABLES_PER_ROOT * DVM_PAGE_SIZE);
    for (unsigned slot = 0; slot < DVM_USER_SLOTS; ++slot) {
        struct dvm_observed got;
        assert(dvm_lookup(&image_arena.tables, &output, slot + 1,
                          plan.user[slot][0].va, &got));
        assert(got.user && got.executable && !got.writable &&
               got.pa == plan.user[slot][0].pa);
        assert(!dvm_lookup(&image_arena.tables, &output, 0,
                           plan.user[slot][0].va, &got));
    }
    setup(); fail_at = 7; rejected(DVO_PROOF, 8);
    setup(); fail_at = 0; rejected(DVO_PROOF, 1);
    setup(); plan.user[1][1].pa = plan.user[0][1].pa; rejected(DVO_LAYOUT, 0);
    setup(); plan.user[1][1].pa += DVM_PAGE_SIZE; rejected(DVO_LAYOUT, 0);
    setup(); plan.physical_table_base += DVM_PAGE_SIZE; rejected(DVO_LAYOUT, 0);
    setup(); plan.kernel[0].pa = base(); rejected(DVO_OVERLAP, 0);
    setup(); evidence.live.image.start = base() + DVM_PAGE_SIZE; rejected(DVO_SCOPE, 0);
    setup(); memory.nr_reserved = 0; rejected(DVO_RESERVED, 0);
    setup(); memory.reserved[0].end = base() + DVM_PAGE_SIZE; rejected(DVO_RESERVED, 0);
    setup(); evidence.live.handoff = (struct boot_range){base(), base() + 16}; rejected(DVO_OVERLAP, 0);
    setup(); excluded.pool = (struct boot_range){base() + DVM_PAGE_SIZE, base() + 2u * DVM_PAGE_SIZE};
    rejected(DVO_OVERLAP, 0);
    setup(); plan.user[0][0].rights = DVM_WRITE | DVM_EXEC; rejected(DVO_BUILD, DVO_OWNED_PAGES);
    setup(); plan.user[0][0].va = plan.user[0][1].va; rejected(DVO_BUILD, DVO_OWNED_PAGES);
    setup(); const uint64_t b = base();
    assert(dvo_build_owned(&evidence, (struct dvo_arena){b + DVM_PAGE_SIZE, DVO_OWNED_BYTES},
            excluded, &plan, &image_arena.tables, &output, fake_identity, (void *)&b)
           == DVO_ALIGNMENT);
    assert(output.root[0] == 0);
    puts("DESKTOP VM OWNER HOST PASS: exact image-backed arena, witness, reserved map, isolation and fail-closed roots");
    return 0;
}
