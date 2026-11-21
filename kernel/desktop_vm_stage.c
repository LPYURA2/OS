#include <styleos/desktop_vm_stage.h>

#define DVM_PHYS_LIMIT (UINT64_C(1) << 52)
#define DVM_VA_LIMIT (UINT64_C(1) << 47)
#define DVM_NX (UINT64_C(1) << 63)
#define DVM_ADDRESS UINT64_C(0x000ffffffffff000)
#define DVM_PRESENT UINT64_C(1)
#define DVM_RW UINT64_C(2)
#define DVM_US UINT64_C(4)
#define DVM_PARENT_FLAGS (DVM_PRESENT | DVM_RW)

static void clear_bytes(void *p, size_t n)
{
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < n; ++i) b[i] = 0;
}

static bool region_valid(const struct dvm_region *r, bool user)
{
    if (!r->pages || r->pages > 65536u ||
        (r->rights & ~(DVM_WRITE | DVM_EXEC)) ||
        (r->rights & (DVM_WRITE | DVM_EXEC)) == (DVM_WRITE | DVM_EXEC) ||
        (r->va | r->pa) & (DVM_PAGE_SIZE - 1u)) return false;
    const uint64_t span = (uint64_t)r->pages * DVM_PAGE_SIZE;
    if (r->pa >= DVM_PHYS_LIMIT || span > DVM_PHYS_LIMIT - r->pa ||
        r->va >= DVM_VA_LIMIT || span > DVM_VA_LIMIT - r->va) return false;
    if (user) {
        if (r->pages != 1 || r->va < DVM_USER_BEGIN ||
            r->va >= DVM_USER_END || span > DVM_USER_END - r->va) return false;
    } else if (r->va >= DVM_USER_BEGIN ||
               span > DVM_USER_BEGIN - r->va) return false;
    return true;
}

static bool overlaps(uint64_t a, uint64_t alen, uint64_t b, uint64_t blen)
{ return a < b + blen && b < a + alen; }

static enum dvm_status validate_plan(const struct dvm_plan *plan)
{
    if (!plan || !plan->kernel_count ||
        plan->kernel_count > DVM_KERNEL_MAX_SPANS) return DVM_ARGUMENT;
    const uint64_t arena_bytes = sizeof(struct dvm_storage);
    if (!plan->physical_table_base ||
        (plan->physical_table_base & (DVM_PAGE_SIZE - 1u)) ||
        plan->physical_table_base >= DVM_PHYS_LIMIT ||
        arena_bytes > DVM_PHYS_LIMIT - plan->physical_table_base)
        return DVM_ALIGNMENT;
    for (size_t i = 0; i < plan->kernel_count; ++i) {
        const struct dvm_region *a = &plan->kernel[i];
        if (!region_valid(a, false)) return DVM_RANGE;
        for (size_t j = 0; j < i; ++j) {
            const struct dvm_region *b = &plan->kernel[j];
            if (overlaps(a->va, (uint64_t)a->pages * DVM_PAGE_SIZE,
                         b->va, (uint64_t)b->pages * DVM_PAGE_SIZE) ||
                overlaps(a->pa, (uint64_t)a->pages * DVM_PAGE_SIZE,
                         b->pa, (uint64_t)b->pages * DVM_PAGE_SIZE))
                return DVM_ALIAS;
        }
    }
    for (unsigned slot = 0; slot < DVM_USER_SLOTS; ++slot) {
        for (unsigned j = 0; j < DVM_USER_PAGES; ++j) {
            const struct dvm_region *a = &plan->user[slot][j];
            if (!region_valid(a, true)) return DVM_RANGE;
            const uint32_t required = j ? DVM_WRITE : DVM_EXEC;
            if (a->rights != required) return DVM_RIGHTS;
            if (overlaps(a->pa, DVM_PAGE_SIZE,
                         plan->physical_table_base, arena_bytes))
                return DVM_ALIAS;
            for (size_t k = 0; k < plan->kernel_count; ++k) {
                const struct dvm_region *b = &plan->kernel[k];
                if (overlaps(a->pa, DVM_PAGE_SIZE,
                             b->pa, (uint64_t)b->pages * DVM_PAGE_SIZE))
                    return DVM_ALIAS;
            }
            for (unsigned other_slot = 0; other_slot <= slot; ++other_slot) {
                for (unsigned other_j = 0; other_j < DVM_USER_PAGES; ++other_j) {
                    if (other_slot == slot && other_j >= j) break;
                    const struct dvm_region *b = &plan->user[other_slot][other_j];
                    if (a->pa == b->pa ||
                        (other_slot == slot && a->va == b->va)) return DVM_ALIAS;
                }
            }
        }
    }
    for (size_t i = 0; i < plan->kernel_count; ++i) {
        const struct dvm_region *a = &plan->kernel[i];
        if (overlaps(a->pa, (uint64_t)a->pages * DVM_PAGE_SIZE,
                     plan->physical_table_base, arena_bytes)) return DVM_ALIAS;
    }
    return DVM_OK;
}

/* Only the pre-validated arena's own table pages may become child pointers. */
static bool put_leaf(struct dvm_storage *storage,
                     const struct dvm_plan *plan, unsigned root,
                     uint32_t *used, uint64_t va, uint64_t pa,
                     uint32_t rights, bool user)
{
    const uint64_t base = plan->physical_table_base +
                          (uint64_t)root * DVM_TABLES_PER_ROOT * DVM_PAGE_SIZE;
    uint64_t *page = storage->table[root][0];
    for (unsigned depth = 0; depth < 3u; ++depth) {
        const unsigned index = (unsigned)((va >> (39u - 9u * depth)) & 511u);
        uint64_t entry = page[index];
        if (!(entry & DVM_PRESENT)) {
            if (*used >= DVM_TABLES_PER_ROOT) return false;
            const uint64_t child = base + (uint64_t)(*used) * DVM_PAGE_SIZE;
            entry = child | DVM_PARENT_FLAGS;
            ++*used;
        }
        if (entry & (DVM_NX | UINT64_C(128))) return false;
        if (user) entry |= DVM_US;
        page[index] = entry;
        const uint64_t child_pa = entry & DVM_ADDRESS;
        if (child_pa < base || (child_pa - base) % DVM_PAGE_SIZE ||
            (child_pa - base) / DVM_PAGE_SIZE >= *used) return false;
        page = storage->table[root][(child_pa - base) / DVM_PAGE_SIZE];
    }
    const unsigned index = (unsigned)((va >> 12u) & 511u);
    if (page[index]) return false;
    page[index] = pa | DVM_PRESENT | ((rights & DVM_WRITE) ? DVM_RW : 0u) |
                  (user ? DVM_US : 0u) | ((rights & DVM_EXEC) ? 0u : DVM_NX);
    return true;
}

bool dvm_lookup(const struct dvm_storage *storage,
                const struct dvm_output *output, unsigned root,
                uint64_t va, struct dvm_observed *found)
{
    if (!storage || !output || !found || root >= DVM_ROOTS ||
        va >= DVM_VA_LIMIT || !output->root[root] ||
        output->used[root] == 0 || output->used[root] > DVM_TABLES_PER_ROOT)
        return false;
    const uint64_t base = output->root[root];
    uint64_t page_index = 0;
    bool user = true, write = true, exec = true;
    for (unsigned depth = 0; depth < 4; ++depth) {
        const uint64_t entry = storage->table[root][page_index]
            [(va >> (39u - 9u * depth)) & 511u];
        if (!(entry & DVM_PRESENT)) return false;
        user = user && !!(entry & DVM_US);
        write = write && !!(entry & DVM_RW);
        exec = exec && !(entry & DVM_NX);
        const uint64_t addr = entry & DVM_ADDRESS;
        if (depth == 3) {
            *found = (struct dvm_observed){addr | (va & (DVM_PAGE_SIZE - 1u)),
                                            user, write, exec};
            return true;
        }
        if ((entry & UINT64_C(128)) || addr < base ||
            ((addr - base) & (DVM_PAGE_SIZE - 1u))) return false;
        page_index = (addr - base) / DVM_PAGE_SIZE;
        if (page_index >= output->used[root]) return false;
    }
    return false;
}

static bool matches(const struct dvm_storage *storage,
                    const struct dvm_output *out, unsigned root,
                    uint64_t va, uint64_t pa, bool user, uint32_t rights)
{
    struct dvm_observed got;
    return dvm_lookup(storage, out, root, va, &got) &&
           got.pa == pa && got.user == user &&
           got.writable == !!(rights & DVM_WRITE) &&
           got.executable == !!(rights & DVM_EXEC);
}

enum dvm_status dvm_build(const struct dvm_plan *plan,
                          struct dvm_storage *storage,
                          struct dvm_output *out)
{
    if (!storage || !out || (const void *)storage == (const void *)out)
        return DVM_ARGUMENT;
    clear_bytes(out, sizeof(*out));
    const enum dvm_status status = validate_plan(plan);
    if (status != DVM_OK) {
        clear_bytes(storage, sizeof(*storage));
        return status;
    }
    clear_bytes(storage, sizeof(*storage));
    for (unsigned root = 0; root < DVM_ROOTS; ++root) {
        uint32_t used = 1;
        for (size_t i = 0; i < plan->kernel_count; ++i) {
            const struct dvm_region *r = &plan->kernel[i];
            for (uint32_t j = 0; j < r->pages; ++j) {
                if (!put_leaf(storage, plan, root, &used,
                              r->va + (uint64_t)j * DVM_PAGE_SIZE,
                              r->pa + (uint64_t)j * DVM_PAGE_SIZE,
                              r->rights, false)) goto rollback;
            }
        }
        if (root) {
            for (unsigned j = 0; j < DVM_USER_PAGES; ++j) {
                const struct dvm_region *r = &plan->user[root - 1u][j];
                if (!put_leaf(storage, plan, root, &used,
                              r->va, r->pa, r->rights, true)) goto rollback;
            }
        }
        out->root[root] = plan->physical_table_base +
                         (uint64_t)root * DVM_TABLES_PER_ROOT * DVM_PAGE_SIZE;
        out->used[root] = used;
    }
    for (unsigned root = 0; root < DVM_ROOTS; ++root) {
        for (size_t i = 0; i < plan->kernel_count; ++i) {
            const struct dvm_region *r = &plan->kernel[i];
            for (uint32_t j = 0; j < r->pages; ++j)
                if (!matches(storage, out, root,
                             r->va + (uint64_t)j * DVM_PAGE_SIZE,
                             r->pa + (uint64_t)j * DVM_PAGE_SIZE,
                             false, r->rights)) goto rollback;
        }
        for (unsigned slot = 0; slot < DVM_USER_SLOTS; ++slot) {
            for (unsigned j = 0; j < DVM_USER_PAGES; ++j) {
                const struct dvm_region *r = &plan->user[slot][j];
                if (root == slot + 1u) {
                    if (!matches(storage, out, root, r->va, r->pa, true,
                                 r->rights)) goto rollback;
                } else {
                    struct dvm_observed absent;
                    /* Sharing a virtual address between independent roots is
                     * legal. It must resolve to our own PA, never the other
                     * context's physical page. */
                    if (dvm_lookup(storage, out, root, r->va, &absent)) {
                        if (root == 0 || absent.pa == r->pa || !absent.user)
                            goto rollback;
                        bool own = false;
                        for (unsigned k = 0; k < DVM_USER_PAGES; ++k) {
                            const struct dvm_region *mine = &plan->user[root - 1u][k];
                            if (mine->va == r->va && mine->pa == absent.pa &&
                                absent.writable == !!(mine->rights & DVM_WRITE) &&
                                absent.executable == !!(mine->rights & DVM_EXEC))
                                own = true;
                        }
                        if (!own) goto rollback;
                    }
                }
            }
        }
    }
    return DVM_OK;
rollback:
    clear_bytes(storage, sizeof(*storage));
    clear_bytes(out, sizeof(*out));
    return DVM_CAPACITY;
}
