#include <styleos/desktop_vm_stage.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct dvm_storage pages;
static struct dvm_output out;
static struct dvm_plan plan;
static struct dvm_observed found;

static void valid_plan(void)
{
    memset(&plan, 0, sizeof(plan));
    plan.physical_table_base = 0x1000000;
    plan.kernel_count = 2;
    plan.kernel[0] = (struct dvm_region){0x200000, 0x200000, 2, DVM_EXEC};
    plan.kernel[1] = (struct dvm_region){0x202000, 0x202000, 3, DVM_WRITE};
    for (unsigned i = 0; i < DVM_USER_SLOTS; ++i) {
        const uint64_t offset = (uint64_t)i * 0x40000;
        const uint64_t physical = 0x300000 + (uint64_t)i * 0x3000;
        plan.user[i][0] = (struct dvm_region){DVM_USER_BEGIN + offset,
                                               physical, 1, DVM_EXEC};
        plan.user[i][1] = (struct dvm_region){DVM_USER_BEGIN + offset + 0x10000,
                                               physical + 0x1000, 1, DVM_WRITE};
        plan.user[i][2] = (struct dvm_region){DVM_USER_BEGIN + offset + 0x20000,
                                               physical + 0x2000, 1, DVM_WRITE};
    }
}
static void rejected(enum dvm_status wanted)
{
    memset(&pages, 0xa5, sizeof(pages));
    memset(&out, 0x5a, sizeof(out));
    enum dvm_status got = dvm_build(&plan, &pages, &out);
    if (got != wanted) fprintf(stderr, "reject expected=%u got=%u\n", (unsigned)wanted, (unsigned)got);
    assert(got == wanted);
    for (unsigned i = 0; i < DVM_ROOTS; ++i) {
        assert(out.root[i] == 0 && out.used[i] == 0);
        for (unsigned j = 0; j < DVM_TABLES_PER_ROOT; ++j)
            assert(pages.table[i][j][0] == 0);
    }
}
int main(void)
{
    valid_plan();
    assert(dvm_build(&plan, &pages, &out) == DVM_OK);
    assert(out.root[0] == plan.physical_table_base);
    assert(out.root[1] == plan.physical_table_base +
                          (uint64_t)DVM_TABLES_PER_ROOT * DVM_PAGE_SIZE);
    assert(out.root[2] == out.root[1] +
                          (uint64_t)DVM_TABLES_PER_ROOT * DVM_PAGE_SIZE);
    for (unsigned root = 0; root < DVM_ROOTS; ++root) {
        assert(out.used[root] > 1 && out.used[root] <= DVM_TABLES_PER_ROOT);
        assert(dvm_lookup(&pages, &out, root, 0x200000, &found));
        assert(found.pa == 0x200000 && !found.user && !found.writable && found.executable);
        assert(dvm_lookup(&pages, &out, root, 0x202000, &found));
        assert(found.pa == 0x202000 && !found.user && found.writable && !found.executable);
        assert(!dvm_lookup(&pages, &out, root, DVM_USER_END, &found));
        for (unsigned slot = 0; slot < DVM_USER_SLOTS; ++slot) {
            for (unsigned page = 0; page < DVM_USER_PAGES; ++page) {
                const struct dvm_region *r = &plan.user[slot][page];
                const bool present = dvm_lookup(&pages, &out, root, r->va, &found);
                assert(present == (root == slot + 1u));
                if (present) {
                    assert(found.user && found.pa == r->pa);
                    assert(found.writable == (page != 0));
                    assert(found.executable == (page == 0));
                }
            }
        }
    }
    /* Cross-root reuse of VAs is legal: distinct PAs still remain private. */
    plan.user[1][0].va = plan.user[0][0].va;
    assert(dvm_build(&plan, &pages, &out) == DVM_OK);
    assert(dvm_lookup(&pages, &out, 1, plan.user[0][0].va, &found));
    assert(found.pa == plan.user[0][0].pa);
    assert(dvm_lookup(&pages, &out, 2, plan.user[1][0].va, &found));
    assert(found.pa == plan.user[1][0].pa);
    assert(!dvm_lookup(&pages, &out, 0, plan.user[1][0].va, &found));
    valid_plan();

    plan.user[0][0].rights = DVM_WRITE | DVM_EXEC; rejected(DVM_RANGE);
    valid_plan(); plan.user[0][0].rights = DVM_WRITE; rejected(DVM_RIGHTS);
    valid_plan(); plan.user[0][1].va = plan.user[0][0].va; rejected(DVM_ALIAS);
    valid_plan(); plan.user[1][0].pa = plan.user[0][0].pa; rejected(DVM_ALIAS);
    valid_plan(); plan.user[0][0].pa = plan.kernel[0].pa; rejected(DVM_ALIAS);
    valid_plan(); plan.user[0][0].pa = plan.physical_table_base; rejected(DVM_ALIAS);
    valid_plan(); plan.kernel[1].va = plan.kernel[0].va; rejected(DVM_ALIAS);
    valid_plan(); plan.kernel[1].pa = plan.kernel[0].pa; rejected(DVM_ALIAS);
    valid_plan(); plan.physical_table_base |= 1; rejected(DVM_ALIGNMENT);
    valid_plan(); plan.kernel[0].pages = 0; rejected(DVM_RANGE);
    valid_plan(); plan.user[0][0].va = DVM_USER_END; rejected(DVM_RANGE);
    valid_plan(); plan.user[0][0].va = UINT64_MAX - 4095u; rejected(DVM_RANGE);
    valid_plan(); plan.kernel[0].va = DVM_USER_BEGIN; rejected(DVM_RANGE);
    valid_plan(); plan.kernel[0].rights = DVM_WRITE | DVM_EXEC; rejected(DVM_RANGE);
    valid_plan(); plan.kernel[0].va = 0x4000000; plan.kernel[0].pa = 0x4000000;
    plan.kernel[0].pages = 65536; rejected(DVM_CAPACITY);
    valid_plan(); assert(dvm_build(&plan, &pages, &out) == DVM_OK);
    pages.table[0][0][0] = UINT64_C(0xdead000) | 7u;
    assert(!dvm_lookup(&pages, &out, 0, 0x200000, &found));
    assert(!dvm_lookup(0, &out, 0, 0x200000, &found));
    assert(!dvm_lookup(&pages, &out, DVM_ROOTS, 0x200000, &found));
    assert(dvm_build(0, &pages, &out) == DVM_ARGUMENT);
    puts("DESKTOP VM STAGE PASS: finite roots, W^X, private pages, alias refusal, rollback");
    return 0;
}
