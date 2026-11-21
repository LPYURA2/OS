#include <styleos/process_api.h>

static int request_valid(const struct pa_request *r)
{
    if (!r || r->version != PA_VERSION || r->reserved ||
        r->descending > 1u || r->order < PI_BY_PID ||
        r->order > PI_BY_STATE) return 0;
    for (unsigned i = 0; i < PA_SEARCH_BYTES; ++i)
        if (!r->search[i]) return 1;
    return 0;
}

enum pa_status pa_read(const struct pr_table *processes,
                       const struct ws_service *windows,
                       uint32_t kernel_authenticated_pid,
                       bool kernel_inspect_granted,
                       const struct pa_request *request,
                       struct pa_row *rows, size_t capacity,
                       struct pa_reply *reply)
{
    if (!processes || !windows || !request || !reply ||
        capacity > PA_MAX_ROWS || (capacity && !rows))
        return PA_BAD_REQUEST;
    /* Check authorization BEFORE disclosing row counts or modifying buffers. */
    if (!kernel_inspect_granted ||
        !pr_is_live(processes, kernel_authenticated_pid)) return PA_DENIED;
    if (!request_valid(request)) return PA_BAD_REQUEST;

    struct pi_row scratch[PA_MAX_ROWS];
    struct pi_view_info view;
    const struct pi_query q = {
        .by = (enum pi_order)request->order,
        .descending = request->descending != 0,
        .search = request->search,
    };
    if (pi_view(processes, windows, &q, scratch, PA_MAX_ROWS, &view) != PI_OK)
        return PA_BAD_REQUEST;
    const size_t written = view.matched < capacity ? view.matched : capacity;
    for (size_t i = 0; i < written; ++i) {
        const struct pi_row *src = &scratch[i];
        struct pa_row *dst = &rows[i];
        dst->pid = src->pid;
        dst->parent_pid = src->parent_pid;
        dst->group = (uint32_t)src->group;
        dst->type = (uint32_t)src->type;
        dst->state = (uint32_t)src->state;
        dst->open_windows = src->open_windows;
        dst->cpu_ticks = src->cpu_ticks;
        dst->resident_bytes = src->resident_bytes;
        for (unsigned j = 0; j < PA_NAME_BYTES; ++j)
            dst->name[j] = src->name[j];
    }
    *reply = (struct pa_reply){
        .version = PA_VERSION,
        .process_serial = view.source.process_serial,
        .window_generation = view.source.window_generation,
        .total = (uint32_t)view.source.total,
        .matched = (uint32_t)view.matched,
        .returned = (uint32_t)written,
        .applications = (uint32_t)view.source.applications,
        .background = (uint32_t)view.source.background,
        .system = (uint32_t)view.source.system,
        .measurement_flags = 0, /* NO fabricated CPU% or RAM confidence */
    };
    return view.matched > capacity ? PA_TRUNCATED : PA_OK;
}
