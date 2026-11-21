#include <styleos/desktop_frame_owner.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct dvm_storage storage;
static struct dfo_owner owner;
static struct dfo_claims grants;
static struct pr_table registry;
static struct dfo_ticket ticket_a, ticket_b;

static void sample(void)
{
    memset(&grants, 0, sizeof(grants));
    grants.table_pa = 0x1000000;
    grants.kernel_count = 2;
    grants.kernel[0] = (struct dvm_region){0x200000, 0x200000, 2, DVM_EXEC};
    grants.kernel[1] = (struct dvm_region){0x202000, 0x202000, 2, DVM_WRITE};
    for (unsigned s = 0; s < DVM_USER_SLOTS; ++s)
        for (unsigned p = 0; p < DVM_USER_PAGES; ++p)
            grants.user_pa[s][p] = 0x300000 + (uint64_t)(s*3u+p)*DVM_PAGE_SIZE;
}

static void negative(void)
{
    memset(&owner, 0, sizeof(owner));
    memset(&storage, 0x9a, sizeof(storage));
    assert(dfo_prepare(&owner, &storage, &grants) == DFO_BAD_LAYOUT);
    assert(!owner.ready && !owner.roots.root[0]);
    assert(!storage.table[0][0][0] && !storage.table[2][1][0]);
}

int main(void)
{
    sample();
    assert(dfo_prepare(0, &storage, &grants) == DFO_ARGUMENT);
    assert(dfo_prepare(&owner, &storage, &grants) == DFO_OK);
    assert(dfo_prepare(&owner, &storage, &grants) == DFO_ALREADY_READY);
    assert(owner.roots.root[0] == grants.table_pa);
    assert(owner.roots.root[1] != owner.roots.root[2]);
    for (unsigned root = 0; root < DVM_ROOTS; ++root) {
        struct dvm_observed seen;
        for (unsigned s = 0; s < DVM_USER_SLOTS; ++s)
            for (unsigned p = 0; p < DVM_USER_PAGES; ++p) {
                assert(dvm_lookup(&storage, &owner.roots, root,
                    owner.plan.user[s][p].va, &seen) == (root != 0));
                if (root != 0) {
                    assert(seen.pa == grants.user_pa[root-1][p]);
                    assert(seen.user && seen.executable == (p == 0));
                    assert(seen.writable == (p != 0));
                }
            }
    }
    pr_init(&registry);
    uint32_t a = 0, b = 0, wrong = 0;
    struct pr_admission admission = {
        PR_NATIVE_USER, 0, 111, owner.roots.root[1], 0, "Editor.tea"
    };
    assert(pr_admit(&registry, &admission, &a) == PR_OK);
    admission.scheduler_handle = 222;
    admission.page_root = owner.roots.root[2];
    admission.label = "Processes.tea";
    assert(pr_admit(&registry, &admission, &b) == PR_OK);
    admission.scheduler_handle = 333;
    admission.page_root = 0x4000;
    admission.label = "Wrong.tea";
    assert(pr_admit(&registry, &admission, &wrong) == PR_OK);
    assert(dfo_bind(&owner, &registry, 0, wrong, 333, &ticket_a) == DFO_EVIDENCE);
    assert(dfo_bind(&owner, &registry, 0, a, 222, &ticket_a) == DFO_EVIDENCE);
    assert(dfo_bind(&owner, &registry, 0, a, 111, &ticket_a) == DFO_OK);
    assert(dfo_bind(&owner, &registry, 1, b, 222, &ticket_b) == DFO_OK);
    assert(ticket_a.generation && ticket_b.generation > ticket_a.generation);
    assert(dfo_bind(&owner, &registry, 0, a, 111, &ticket_a) == DFO_BUSY);
    assert(dfo_close(&owner, 0x1000) == DFO_BUSY);
    assert(dfo_revoke(&owner, ticket_a, owner.roots.root[1]) == DFO_ACTIVE_ROOT);
    assert(dfo_revoke(&owner, ticket_a, 0x1000) == DFO_BUSY);
    struct dfo_ticket altered = ticket_a;
    ++altered.generation;
    assert(dfo_revoke(&owner, altered, 0x1000) == DFO_STALE);
    assert(pr_transition(&registry, a, 111, PR_RUNNING) == PR_OK);
    assert(pr_transition(&registry, a, 111, PR_EXITED) == PR_OK);
    assert(dfo_revoke(&owner, ticket_a, 0x1000) == DFO_OK);
    assert(owner.roots.root[1] == 0 && owner.roots.used[1] == 0);
    assert(!storage.table[1][0][0]);
    assert(dfo_revoke(&owner, ticket_a, 0x1000) == DFO_STALE);
    assert(dfo_close(&owner, 0x1000) == DFO_BUSY);
    assert(dfo_bind(&owner, &registry, 0, a, 111, &ticket_a) == DFO_BUSY);
    assert(pr_transition(&registry, b, 222, PR_FAULTED) == PR_OK);
    assert(dfo_revoke(&owner, ticket_b, 0x1000) == DFO_OK);
    assert(dfo_close(&owner, owner.roots.root[0]) == DFO_ACTIVE_ROOT);
    assert(dfo_close(&owner, 0x1000) == DFO_OK);
    assert(!storage.table[0][0][0] && !owner.ready);
    assert(pr_reap(&registry, a, 111) == PR_OK);
    assert(pr_reap(&registry, b, 222) == PR_OK);
    /* Any slot can be opened once only per prepared arena. The owner does not
     * return user pages to any allocator until an external arch teardown. */
    sample(); grants.user_pa[1][0] = grants.user_pa[0][0]; negative();
    sample(); grants.user_pa[0][1] = grants.table_pa; negative();
    sample(); grants.user_pa[0][0] |= 1; negative();
    sample(); grants.user_pa[0][2] = grants.kernel[0].pa; negative();
    sample(); grants.table_pa |= 1; negative();
    sample(); grants.kernel[0].rights = DVM_WRITE | DVM_EXEC; negative();
    sample(); grants.kernel_count = 0; negative();
    sample(); grants.kernel[0].pages = 65536; negative();
    sample(); assert(dfo_prepare(&owner, &storage, &grants) == DFO_OK);
    assert(dfo_close(&owner, 0x1000) == DFO_BUSY); /* not revoked */
    puts("DESKTOP FRAME OWNER HOST PASS: grants, isolation, PID leases, active-root guards, revocation, rollback");
    return 0;
}
