#include <styleos/desktop_frame_owner.h>

#define DFO_PHYS_LIMIT (UINT64_C(1) << 52)
#define DFO_PAGE_MASK (DVM_PAGE_SIZE - 1u)

static void wipe(void *ptr, size_t bytes)
{
    unsigned char *p = (unsigned char *)ptr;
    for (size_t i = 0; i < bytes; ++i) p[i] = 0;
}

static bool frame_ok(uint64_t pa)
{
    return pa && !(pa & DFO_PAGE_MASK) &&
           pa <= DFO_PHYS_LIMIT - DVM_PAGE_SIZE;
}

static bool collide(uint64_t a, uint64_t bytes_a,
                    uint64_t b, uint64_t bytes_b)
{
    return a < b + bytes_b && b < a + bytes_a;
}

/* This is a structural check of *claims*, not physical ownership evidence.
 * The trusted platform must prove the grants before passing them here. */
static bool claims_ok(const struct dfo_claims *c)
{
    if (!c || !frame_ok(c->table_pa) ||
        c->kernel_count == 0 || c->kernel_count > DVM_KERNEL_MAX_SPANS ||
        sizeof(struct dvm_storage) > DFO_PHYS_LIMIT - c->table_pa)
        return false;
    for (unsigned s = 0; s < DVM_USER_SLOTS; ++s) {
        for (unsigned p = 0; p < DVM_USER_PAGES; ++p) {
            const uint64_t frame = c->user_pa[s][p];
            if (!frame_ok(frame) ||
                collide(frame, DVM_PAGE_SIZE,
                        c->table_pa, sizeof(struct dvm_storage))) return false;
            for (unsigned a = 0; a <= s; ++a) {
                for (unsigned b = 0; b < DVM_USER_PAGES; ++b) {
                    if (a == s && b >= p) break;
                    if (frame == c->user_pa[a][b]) return false;
                }
            }
        }
    }
    return true;
}

static void make_plan(struct dvm_plan *p, const struct dfo_claims *c)
{
    wipe(p, sizeof(*p));
    p->physical_table_base = c->table_pa;
    p->kernel_count = c->kernel_count;
    for (size_t i = 0; i < c->kernel_count; ++i) p->kernel[i] = c->kernel[i];
    for (unsigned slot = 0; slot < DVM_USER_SLOTS; ++slot) {
        /* Same VAs in each user root are intentional: separate PAs. */
        p->user[slot][0] = (struct dvm_region){
            DVM_USER_BEGIN, c->user_pa[slot][0], 1, DVM_EXEC};
        p->user[slot][1] = (struct dvm_region){
            DVM_USER_BEGIN + 0x10000u, c->user_pa[slot][1], 1, DVM_WRITE};
        p->user[slot][2] = (struct dvm_region){
            DVM_USER_BEGIN + 0x20000u, c->user_pa[slot][2], 1, DVM_WRITE};
    }
}

enum dfo_status dfo_prepare(struct dfo_owner *o, struct dvm_storage *storage,
                            const struct dfo_claims *claims)
{
    if (!o || !storage || !claims ||
        (void *)o == (void *)storage) return DFO_ARGUMENT;
    if (o->ready) return DFO_ALREADY_READY;
    /* Do not let caller-controlled physical frames reach the builder until
     * all of the finite claims are structurally valid. */
    if (!claims_ok(claims)) {
        wipe(storage, sizeof(*storage));
        wipe(o, sizeof(*o));
        return DFO_BAD_LAYOUT;
    }
    struct dvm_plan candidate;
    struct dvm_output roots;
    make_plan(&candidate, claims);
    if (dvm_build(&candidate, storage, &roots) != DVM_OK) {
        wipe(storage, sizeof(*storage));
        wipe(o, sizeof(*o));
        return DFO_BAD_LAYOUT;
    }
    wipe(o, sizeof(*o));
    o->storage = storage;
    o->plan = candidate;
    o->roots = roots;
    o->ready = true;
    return DFO_OK;
}

enum dfo_status dfo_bind(struct dfo_owner *o, const struct pr_table *registry,
                         unsigned slot, uint32_t pid, uint64_t handle,
                         struct dfo_ticket *out)
{
    if (!o || !registry || !out || slot >= DVM_USER_SLOTS || !pid || !handle)
        return DFO_ARGUMENT;
    if (!o->ready) return DFO_NOT_READY;
    if (o->lease[slot].bound || o->lease[slot].revoked ||
        (o->registry && o->registry != registry)) return DFO_BUSY;
    const struct pr_entry *entry = pr_find(registry, pid);
    if (!entry || entry->type != PR_NATIVE_USER ||
        entry->state != PR_READY || entry->page_root != o->roots.root[slot + 1u] ||
        entry->scheduler_handle != handle) return DFO_EVIDENCE;
    for (unsigned i = 0; i < DVM_USER_SLOTS; ++i)
        if (o->lease[i].bound && o->lease[i].pid == pid) return DFO_EVIDENCE;
    if (o->next_generation == UINT32_MAX) return DFO_EXHAUSTED;
    const uint32_t generation = ++o->next_generation;
    o->lease[slot] = (struct dfo_lease){pid, generation, handle, true, false};
    o->registry = registry;
    *out = (struct dfo_ticket){pid, generation, slot};
    return DFO_OK;
}

enum dfo_status dfo_revoke(struct dfo_owner *o, struct dfo_ticket ticket,
                           uint64_t observed_cr3)
{
    if (!o || ticket.slot >= DVM_USER_SLOTS || !ticket.pid ||
        !ticket.generation || !frame_ok(observed_cr3)) return DFO_ARGUMENT;
    if (!o->ready || !o->registry) return DFO_NOT_READY;
    struct dfo_lease *lease = &o->lease[ticket.slot];
    if (!lease->bound || lease->revoked ||
        lease->generation != ticket.generation || lease->pid != ticket.pid)
        return DFO_STALE;
    if (observed_cr3 == o->roots.root[ticket.slot + 1u]) return DFO_ACTIVE_ROOT;
    const struct pr_entry *entry = pr_find(o->registry, ticket.pid);
    if (!entry || entry->scheduler_handle != lease->hardware_context ||
        entry->type != PR_NATIVE_USER ||
        entry->page_root != o->roots.root[ticket.slot + 1u] ||
        (entry->state != PR_EXITED && entry->state != PR_FAULTED))
        return DFO_BUSY;
    /* Root is no longer publishable, and all its table pages are scrubbed.
     * No user physical pages are freed or reissued here. */
    wipe(o->storage->table[ticket.slot + 1u],
         sizeof(o->storage->table[ticket.slot + 1u]));
    o->roots.root[ticket.slot + 1u] = 0;
    o->roots.used[ticket.slot + 1u] = 0;
    lease->bound = false;
    lease->revoked = true;
    return DFO_OK;
}

enum dfo_status dfo_close(struct dfo_owner *o, uint64_t observed_cr3)
{
    if (!o || !frame_ok(observed_cr3)) return DFO_ARGUMENT;
    if (!o->ready || !o->storage) return DFO_NOT_READY;
    if (observed_cr3 == o->roots.root[0]) return DFO_ACTIVE_ROOT;
    for (unsigned s = 0; s < DVM_USER_SLOTS; ++s) {
        if (o->lease[s].bound || !o->lease[s].revoked ||
            observed_cr3 == o->roots.root[s+1u]) return DFO_BUSY;
    }
    wipe(o->storage, sizeof(*o->storage));
    wipe(o, sizeof(*o));
    return DFO_OK;
}
