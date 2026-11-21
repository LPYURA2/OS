#include <styleos/native_launch_stage.h>

static void clear(void *dst, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    for (size_t i = 0; i < n; ++i) p[i] = 0;
}

static bool idle_root(const struct nla_stage *s, uint64_t root)
{
    return s && s->processes && s->processes->dispatcher.current == -1 &&
           root && !(root & (DVM_PAGE_SIZE - 1u)) &&
           root == s->processes->dispatcher.safe_root &&
           root != s->frames->roots.root[0] &&
           root != s->frames->roots.root[1] &&
           root != s->frames->roots.root[2];
}

static bool consistent(const struct nla_stage *s)
{
    return s && s->ready && s->processes && s->processes->started &&
           s->frames && s->frames->ready && s->sessions && s->sessions->ready &&
           s->processes->dispatcher.registry == s->sessions->processes &&
           s->processes->windows == s->sessions->windows &&
           (!s->frames->registry ||
            s->frames->registry == s->sessions->processes);
}

enum nla_status nla_init(struct nla_stage *s, struct dpr_service *p,
                         struct dfo_owner *f, struct usr_runtime *u)
{
    if (!s || !p || !f || !u) return NLA_ARGUMENT;
    if (s->ready) return NLA_CONFLICT;
    if (!p->started || !f->ready || !u->ready ||
        p->dispatcher.registry != u->processes ||
        p->windows != u->windows ||
        p->dispatcher.current != -1 ||
        (f->registry && f->registry != u->processes)) return NLA_NOT_READY;
    for (unsigned i=0; i<DVM_USER_SLOTS; ++i)
        if (f->lease[i].bound || f->lease[i].revoked ||
            !f->roots.root[i+1u] ||
            f->roots.root[i+1u] == p->dispatcher.safe_root)
            return NLA_CONFLICT;
    clear(s, sizeof(*s));
    s->processes = p; s->frames = f; s->sessions = u; s->ready = true;
    return NLA_OK;
}

/* Failure path is deliberately fail-closed. A PID admitted to the READY state
 * must be terminated and reaped; never delete it from pr_table by memcpy.
 * If the architecture has made the user context runnable, this routine MUST
 * NOT be called. The entire transaction is required to hold a scheduler lock. */
static enum nla_status rollback(struct nla_stage *s, unsigned slot,
                                 uint32_t pid, uint64_t handle,
                                 bool bound, struct dfo_ticket ticket,
                                 uint64_t root)
{
    struct pr_table *p = s->processes->dispatcher.registry;
    if (pr_transition(p, pid, handle, PR_EXITED) != PR_OK)
        return NLA_ROLLBACK_FAILED;
    (void)usr_reconcile(s->sessions); /* app windows and user binding */
    if (bound && dfo_revoke(s->frames, ticket, root) != DFO_OK)
        return NLA_ROLLBACK_FAILED;
    if (dpr_release(s->processes, slot, handle) != PS_OK)
        return NLA_ROLLBACK_FAILED;
    return NLA_OK;
}

enum nla_status nla_commit(struct nla_stage *s, const struct nla_request *req,
                           uint64_t observed_kernel_root,
                           struct nla_result *result)
{
    if (!s || !req || !result || req->slot >= DVM_USER_SLOTS ||
        !req->session_id || !req->hardware_context || !req->label ||
        !req->window_title) return NLA_ARGUMENT;
    if (!consistent(s)) return NLA_NOT_READY;
    const unsigned slot = req->slot;
    if (s->entries[slot].committed || s->frames->lease[slot].bound ||
        s->frames->lease[slot].revoked || !s->frames->roots.root[slot+1u])
        return NLA_CONFLICT;
    if (!idle_root(s, observed_kernel_root)) return NLA_CONTEXT;
    /* A user session must exist, but usr_bind independently checks it is the
     * currently authenticated/selected one. This permits rollback testing. */
    if (!usr_find(s->sessions, req->session_id)) return NLA_SESSION;
    const uint64_t user_root = s->frames->roots.root[slot+1u];
    uint32_t pid = 0;
    if (dpr_admit(s->processes, slot, req->hardware_context, user_root,
                  req->parent_pid, req->label, &pid) != PS_OK)
        return NLA_ADMISSION;

    struct dfo_ticket ticket = {0};
    bool bound = false;
    enum nla_status failure = NLA_FRAMES;
    if (dfo_bind(s->frames, s->sessions->processes, slot, pid,
                 req->hardware_context, &ticket) != DFO_OK) goto failed;
    bound = true;
    failure = NLA_SESSION;
    if (usr_bind(s->sessions, req->session_id, pid,
                 req->hardware_context) != USR_OK) goto failed;
    failure = NLA_WINDOW;
    uint32_t window_id = 0;
    if (ws_open_app(s->sessions->windows, s->sessions->processes, pid,
                    req->window_title, req->bounds, &window_id) != WS_OK)
        goto failed;
    s->entries[slot] = (struct nla_record){pid, req->session_id, window_id,
                                            req->hardware_context, ticket, true};
    *result = (struct nla_result){pid, window_id, ticket};
    return NLA_OK;
failed:
    if (rollback(s, slot, pid, req->hardware_context, bound, ticket,
                 observed_kernel_root) != NLA_OK) return NLA_ROLLBACK_FAILED;
    return failure;
}

enum nla_status nla_finalize(struct nla_stage *s, unsigned slot,
                             uint64_t handle, uint64_t observed_kernel_root)
{
    if (!s || slot >= DVM_USER_SLOTS || !handle) return NLA_ARGUMENT;
    if (!consistent(s)) return NLA_NOT_READY;
    struct nla_record *r = &s->entries[slot];
    if (!r->committed || handle != r->handle) return NLA_STALE;
    if (!idle_root(s, observed_kernel_root)) return NLA_CONTEXT;
    const struct pr_entry *p = pr_find(s->sessions->processes, r->pid);
    if (!p || p->scheduler_handle != r->handle ||
        (p->state != PR_EXITED && p->state != PR_FAULTED))
        return NLA_NOT_TERMINATED;
    /* No memory is reclaimed by these steps. The caller separately owns the
     * physical code/stack/data pages and the architecture context teardown. */
    (void)usr_reconcile(s->sessions);
    if (dfo_revoke(s->frames, r->ticket, observed_kernel_root) != DFO_OK)
        return NLA_ROLLBACK_FAILED;
    if (dpr_release(s->processes, slot, handle) != PS_OK)
        return NLA_ROLLBACK_FAILED;
    clear(r, sizeof(*r));
    return NLA_OK;
}
