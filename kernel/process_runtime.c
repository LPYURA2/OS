#include <styleos/process_runtime.h>

static struct pr_table global_processes;
static bool process_global_ready;
struct pr_table *pr_global(void)
{
    if (!process_global_ready) {
        pr_init(&global_processes);
        process_global_ready = true;
    }
    return &global_processes;
}
static void zero(void *ptr, size_t count)
{
    unsigned char *p = ptr;
    for (size_t i = 0; i < count; ++i) p[i] = 0;
}
static int valid_label(const char *name)
{
    if (!name) return 0;
    for (unsigned i = 0; i < PR_LABEL_BYTES; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!c) return i > 0;
        if (c < 0x20 || c == 0x7f) return 0;
    }
    return 0;
}
static int live(enum pr_state s)
{ return s == PR_READY || s == PR_RUNNING || s == PR_WAITING; }
static struct pr_entry *find_mutable(struct pr_table *t, uint32_t pid)
{
    if (!t || !pid) return 0;
    for (unsigned i = 0; i < PR_MAX_PROCESSES; ++i)
        if (t->processes[i].state != PR_UNUSED && t->processes[i].pid == pid)
            return &t->processes[i];
    return 0;
}
void pr_init(struct pr_table *t)
{
    if (!t) return;
    zero(t, sizeof(*t));
    t->next_pid = 1;
}
const struct pr_entry *pr_find(const struct pr_table *t, uint32_t pid)
{
    if (!t || !pid) return 0;
    for (unsigned i = 0; i < PR_MAX_PROCESSES; ++i)
        if (t->processes[i].state != PR_UNUSED && t->processes[i].pid == pid)
            return &t->processes[i];
    return 0;
}
bool pr_is_live(const struct pr_table *t, uint32_t pid)
{
    const struct pr_entry *p = pr_find(t, pid);
    return p && live(p->state);
}
enum pr_status pr_admit(struct pr_table *t, const struct pr_admission *s,
                        uint32_t *pid)
{
    if (!t || !s || !pid || !valid_label(s->label)) return PR_BAD_ARGUMENT;
    /* Trusted scheduler callback provides a real context handle; this is a
     * kernel-only interface, never a registration syscall from an app. */
    if (!s->scheduler_handle ||
        (s->type != PR_KERNEL_TASK && s->type != PR_NATIVE_USER) ||
        (s->type == PR_NATIVE_USER &&
         (!s->page_root || (s->page_root & 4095u))) ||
        (s->type == PR_KERNEL_TASK && s->page_root)) return PR_BAD_EVIDENCE;
    if (s->parent_pid && !pr_is_live(t, s->parent_pid)) return PR_BAD_EVIDENCE;
    if (!t->next_pid || t->next_pid == UINT32_MAX) return PR_EXHAUSTED;
    struct pr_entry *free_slot = 0;
    for (unsigned i = 0; i < PR_MAX_PROCESSES; ++i) {
        struct pr_entry *e = &t->processes[i];
        if (e->state == PR_UNUSED && !free_slot) free_slot = e;
        if (e->state != PR_UNUSED && e->scheduler_handle == s->scheduler_handle)
            return PR_CONFLICT;
    }
    if (!free_slot) return PR_FULL;
    zero(free_slot, sizeof(*free_slot));
    free_slot->pid = t->next_pid++;
    free_slot->parent_pid = s->parent_pid;
    free_slot->type = s->type;
    free_slot->state = PR_READY;
    free_slot->scheduler_handle = s->scheduler_handle;
    free_slot->page_root = s->page_root;
    free_slot->resident_bytes = s->resident_bytes;
    for (unsigned i = 0; i < PR_LABEL_BYTES; ++i) {
        free_slot->label[i] = s->label[i];
        if (!s->label[i]) break;
    }
    *pid = free_slot->pid;
    ++t->change_serial;
    return PR_OK;
}
enum pr_status pr_transition(struct pr_table *t, uint32_t pid,
                             uint64_t handle, enum pr_state next)
{
    struct pr_entry *e = find_mutable(t, pid);
    if (!e) return PR_NOT_FOUND;
    if (!handle || handle != e->scheduler_handle) return PR_BAD_EVIDENCE;
    if (!live(e->state)) return PR_INVALID_STATE;
    if (next != PR_RUNNING && next != PR_READY && next != PR_WAITING &&
        next != PR_EXITED && next != PR_FAULTED) return PR_INVALID_STATE;
    if (next == PR_RUNNING && e->state == PR_RUNNING) return PR_INVALID_STATE;
    e->state = next;
    ++t->change_serial;
    return PR_OK;
}
enum pr_status pr_account(struct pr_table *t, uint32_t pid,
                          uint64_t handle, uint64_t ticks, uint64_t memory)
{
    struct pr_entry *e = find_mutable(t, pid);
    if (!e) return PR_NOT_FOUND;
    if (!handle || handle != e->scheduler_handle) return PR_BAD_EVIDENCE;
    if (!live(e->state)) return PR_INVALID_STATE;
    if (ticks > UINT64_MAX - e->cpu_ticks) return PR_EXHAUSTED;
    e->cpu_ticks += ticks;
    e->resident_bytes = memory;
    ++t->change_serial;
    return PR_OK;
}
enum pr_status pr_reap(struct pr_table *t, uint32_t pid,
                       uint64_t handle)
{
    struct pr_entry *e = find_mutable(t, pid);
    if (!e) return PR_NOT_FOUND;
    if (!handle || handle != e->scheduler_handle) return PR_BAD_EVIDENCE;
    if (e->state != PR_EXITED && e->state != PR_FAULTED) return PR_INVALID_STATE;
    for (unsigned i = 0; i < PR_MAX_PROCESSES; ++i)
        if (t->processes[i].state != PR_UNUSED &&
            t->processes[i].parent_pid == pid)
            t->processes[i].parent_pid = 0;
    zero(e, sizeof(*e));
    ++t->change_serial;
    return PR_OK;
}
size_t pr_snapshot(const struct pr_table *t, struct pr_entry *out,
                   size_t capacity, bool include_terminated)
{
    if (!t || (capacity && !out)) return 0;
    size_t count = 0;
    for (unsigned i = 0; i < PR_MAX_PROCESSES; ++i) {
        const struct pr_entry *e = &t->processes[i];
        if (e->state == PR_UNUSED || (!include_terminated && !live(e->state)))
            continue;
        if (count < capacity) out[count] = *e;
        ++count;
    }
    return count;
}

#if (defined(STYLEOS_TEST_RUNTIME_TICK_860) || defined(STYLEOS_OWNED_SHELL_865))
#include <styleos/runtime_tick_860.h>
enum pr_status pr_tick_860(struct pr_table *t,uint32_t pid,uint64_t handle,uint64_t root)
{
    struct pr_entry *e=find_mutable(t,pid);
    if(!e)return PR_NOT_FOUND;
    if(e->type!=PR_NATIVE_USER||!handle||handle!=e->scheduler_handle||
       !root||(root&4095u)||root!=e->page_root)return PR_BAD_EVIDENCE;
    if(e->state!=PR_RUNNING)return PR_INVALID_STATE;
    if(e->cpu_ticks==UINT64_MAX||t->change_serial==UINT32_MAX)return PR_EXHAUSTED;
    ++e->cpu_ticks;
    e->state=PR_READY;
    ++t->change_serial;
    return PR_OK;
}
#endif
