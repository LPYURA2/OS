#include <styleos/process_scheduler.h>
#include <limits.h>

static int valid_root(uint64_t root)
{ return root && !(root & UINT64_C(4095)); }

static const struct pr_entry *verified(const struct ps_dispatcher *d,
                                       const struct ps_slot *s)
{
    const struct pr_entry *p = pr_find(d->registry, s->pid);
    return p && p->type == PR_NATIVE_USER &&
           p->scheduler_handle == s->handle && p->page_root == s->user_root
           ? p : 0;
}

enum ps_status ps_init(struct ps_dispatcher *d, struct pr_table *registry,
                       uint64_t safe_root)
{
    if (!d || !registry || !valid_root(safe_root)) return PS_ARGUMENT;
    *d = (struct ps_dispatcher){0};
    d->registry = registry;
    d->safe_root = safe_root;
    d->current = -1;
    return PS_OK;
}

enum ps_status ps_register(struct ps_dispatcher *d, unsigned slot,
                           uint64_t handle, uint64_t user_root,
                           uint32_t parent_pid, const char *name,
                           uint32_t *new_pid)
{
    if (!d || !d->registry || !new_pid || !name ||
        slot >= PS_MAX_SLOTS) return PS_ARGUMENT;
    if (d->current != -1 || d->slots[slot].pid || !handle ||
        !valid_root(user_root) || user_root == d->safe_root) return PS_STATE;
    for (unsigned i = 0; i < PS_MAX_SLOTS; ++i)
        if (d->slots[i].pid && (d->slots[i].handle == handle ||
                                d->slots[i].user_root == user_root))
            return PS_EVIDENCE;
    const struct pr_admission a = { PR_NATIVE_USER, parent_pid,
                                    handle, user_root, 0, name };
    uint32_t pid = 0;
    if (pr_admit(d->registry, &a, &pid) != PR_OK) return PS_REGISTRY_ERROR;
    d->slots[slot] = (struct ps_slot){pid, handle, user_root};
    *new_pid = pid;
    return PS_OK;
}

enum ps_status ps_next(struct ps_dispatcher *d, unsigned *slot, uint32_t *pid)
{
    if (!d || !d->registry || !slot || !pid || d->next >= PS_MAX_SLOTS)
        return PS_ARGUMENT;
    if (d->current != -1) return PS_STATE;
    for (unsigned offset = 0; offset < PS_MAX_SLOTS; ++offset) {
        const unsigned i = (d->next + offset) % PS_MAX_SLOTS;
        const struct ps_slot *s = &d->slots[i];
        if (!s->pid) continue;
        const struct pr_entry *p = verified(d, s);
        if (!p) return PS_EVIDENCE;
        if (p->state != PR_READY) continue;
        if (d->dispatches == UINT_MAX) return PS_EXHAUSTED;
        if (pr_transition(d->registry, s->pid, s->handle, PR_RUNNING) != PR_OK)
            return PS_REGISTRY_ERROR;
        d->current = (int)i;
        d->next = (i + 1u) % PS_MAX_SLOTS;
        ++d->dispatches;
        *slot = i;
        *pid = s->pid;
        return PS_OK;
    }
    return PS_EMPTY;
}

enum ps_status ps_accept(struct ps_dispatcher *d, unsigned slot,
                         uint64_t handle, uint64_t observed_user_root,
                         uint64_t observed_safe_root, enum ps_event event)
{
    if (!d || !d->registry || slot >= PS_MAX_SLOTS ||
        (event != PS_YIELD && event != PS_EXIT && event != PS_FAULT))
        return PS_ARGUMENT;
    if (d->current != (int)slot) return PS_STATE;
    const struct ps_slot *s = &d->slots[slot];
    const struct pr_entry *p = verified(d, s);
    if (!p || p->state != PR_RUNNING || handle != s->handle ||
        observed_user_root != s->user_root ||
        observed_safe_root != d->safe_root) return PS_EVIDENCE;
    const enum pr_state target = event == PS_YIELD ? PR_READY :
                                 event == PS_EXIT ? PR_EXITED : PR_FAULTED;
    if (pr_transition(d->registry, s->pid, s->handle, target) != PR_OK)
        return PS_REGISTRY_ERROR;
    d->current = -1;
    return PS_OK;
}

enum ps_status ps_reap(struct ps_dispatcher *d, unsigned slot, uint64_t handle)
{
    if (!d || !d->registry || slot >= PS_MAX_SLOTS) return PS_ARGUMENT;
    if (d->current != -1 || !d->slots[slot].pid) return PS_STATE;
    struct ps_slot *s = &d->slots[slot];
    const struct pr_entry *p = verified(d, s);
    if (!p || handle != s->handle ||
        (p->state != PR_FAULTED && p->state != PR_EXITED)) return PS_EVIDENCE;
    if (pr_reap(d->registry, s->pid, s->handle) != PR_OK)
        return PS_REGISTRY_ERROR;
    *s = (struct ps_slot){0};
    return PS_OK;
}
