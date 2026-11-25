#include <styleos/desktop_process_runtime.h>

static struct dpr_service boot_service;

enum dpr_status dpr_start(struct dpr_service *s, struct pr_table *p,
                          struct ws_service *w, uint64_t root)
{
    if (!s || !p || !w || !root || (root & UINT64_C(4095)))
        return DPR_ARGUMENT;
    if (s->started) return DPR_ALREADY_STARTED;
    /* No migration from fake desktop session ids, no silent reset of state. */
    if (pr_snapshot(p, 0, 0, true) || ws_snapshot(w, 0, 0))
        return DPR_DIRTY;
    struct ps_dispatcher dispatcher;
    if (ps_init(&dispatcher, p, root) != PS_OK) return DPR_SCHEDULER;
    s->dispatcher = dispatcher;
    s->windows = w;
    s->started = true;
    return DPR_OK;
}

enum ps_status dpr_admit(struct dpr_service *s, unsigned slot,
                         uint64_t handle, uint64_t root, uint32_t parent,
                         const char *name, uint32_t *pid)
{
    if (!s || !s->started) return PS_STATE;
    return ps_register(&s->dispatcher, slot, handle, root, parent, name, pid);
}

enum ps_status dpr_dispatch(struct dpr_service *s, unsigned *slot,
                            uint32_t *pid)
{
    if (!s || !s->started) return PS_STATE;
    return ps_next(&s->dispatcher, slot, pid);
}

enum ps_status dpr_observe(struct dpr_service *s, unsigned slot,
                           uint64_t handle, uint64_t user_root,
                           uint64_t safe_root, enum ps_event event)
{
    if (!s || !s->started) return PS_STATE;
    const enum ps_status status = ps_accept(&s->dispatcher, slot, handle,
                                           user_root, safe_root, event);
    if (status == PS_OK && event != PS_YIELD)
        ws_reap_dead_owners(s->windows, s->dispatcher.registry);
    return status;
}

enum ps_status dpr_release(struct dpr_service *s, unsigned slot,
                           uint64_t handle)
{
    if (!s || !s->started) return PS_STATE;
    /* This function is NOT the arch release operation; the caller must prove
     * that the user memory, mapping and hardware context were released. */
    return ps_reap(&s->dispatcher, slot, handle);
}

enum pa_status dpr_snapshot(struct dpr_service *s, uint32_t caller,
                            bool granted, const struct pa_request *query,
                            struct pa_row *rows, size_t capacity,
                            struct pa_reply *reply)
{
    if (!s || !s->started) return PA_DENIED;
    return pa_read(s->dispatcher.registry, s->windows, caller, granted,
                   query, rows, capacity, reply);
}

enum dpr_status dpr_global_start(uint64_t safe_root)
{
    return dpr_start(&boot_service, pr_global(), ws_global(), safe_root);
}

const struct dpr_service *dpr_global_state(void)
{
    return boot_service.started ? &boot_service : 0;
}

#ifdef STYLEOS_TEST_RUNTIME_TICK_860
struct dpr_service *dpr_tick_global_860(void)
{ return boot_service.started ? &boot_service : 0; }
#endif
