#include "styleos/tasks.h"
#include <limits.h>

/* This is only the two-slot bookkeeping engine. Numeric stack intervals are
 * checked here, but stack bytes and machine contexts are never executed or
 * initialized here. The image-owned demo supplies their architectural proof. */
static void clear_bytes(void *object, size_t bytes)
{
    unsigned char *p = object;
    for (size_t i = 0; i < bytes; ++i) p[i] = 0;
}

static int overlaps(struct boot_range a, struct boot_range b)
{
    return a.start < b.end && b.start < a.end;
}

static int control_pointer(const struct tasks_control *control)
{
    uintptr_t p = (uintptr_t)control;
    return control && !(p % _Alignof(struct tasks_control)) &&
           p <= UINTPTR_MAX - sizeof *control;
}

static enum tasks_error stack_range(struct boot_range stack)
{
    if (stack.start > UINT64_MAX - TASKS_STACK_BYTES) return TASKS_OVERFLOW;
    if (stack.start >= stack.end ||
        stack.end - stack.start != TASKS_STACK_BYTES) return TASKS_STACK;
    if ((stack.start | stack.end) & (BOOT_MEMORY_PAGE - 1u))
        return TASKS_ALIGNMENT;
    return TASKS_OK;
}

/* Validate every slot before any operation, including slots the operation
 * would otherwise skip. No malformed current index is used to index slots. */
static enum tasks_error check_control(const struct tasks_control *control)
{
    if (!control_pointer(control)) return TASKS_ARGUMENT;
    if (control->current < -1 || control->current >= (int)TASKS_MAX ||
        control->next >= TASKS_MAX) return TASKS_STATE;

    unsigned running = 0;
    for (unsigned i = 0; i < TASKS_MAX; ++i) {
        const struct tasks_slot *slot = &control->slots[i];
        switch (slot->state) {
        case TASKS_FREE:
            if (slot->entry || slot->stack.start || slot->stack.end ||
                slot->yields) return TASKS_STATE;
            for (unsigned r = 0; r < 12; ++r)
                if (slot->context.gpr[r]) return TASKS_STATE;
            if (slot->context.sp || slot->context.pc) return TASKS_STATE;
            break;
        case TASKS_READY:
        case TASKS_RUNNING:
        case TASKS_DONE:
            if (!slot->entry || stack_range(slot->stack) != TASKS_OK)
                return TASKS_STATE;
            if (slot->state == TASKS_RUNNING) {
                ++running;
                if (control->current != (int)i) return TASKS_STATE;
            }
            for (unsigned j = 0; j < i; ++j)
                if (control->slots[j].state != TASKS_FREE &&
                    overlaps(slot->stack, control->slots[j].stack))
                    return TASKS_STATE;
            break;
        default:
            return TASKS_STATE;
        }
    }
    if ((control->current == -1 && running != 0) ||
        (control->current != -1 && running != 1)) return TASKS_STATE;
    return TASKS_OK;
}

static int output_pointer(const struct tasks_control *control, unsigned *id)
{
    uintptr_t p = (uintptr_t)id;
    uintptr_t start = (uintptr_t)control;
    if (!id || p % _Alignof(unsigned) || p > UINTPTR_MAX - sizeof *id)
        return 0;
    /* An output alias must not turn an otherwise valid operation into a write
     * to its own slot, current index or round-robin cursor. */
    return !(p < start + sizeof *control && start < p + sizeof *id);
}

void tasks_init(struct tasks_control *control)
{
    if (!control_pointer(control)) return;
    clear_bytes(control, sizeof *control);
    control->current = -1;
}

enum tasks_error tasks_create(struct tasks_control *control, tasks_entry entry,
                              struct boot_range stack, unsigned *id)
{
    enum tasks_error error = check_control(control);
    if (error != TASKS_OK) return error;
    if (!entry || !output_pointer(control, id)) return TASKS_ARGUMENT;
    if (control->current != -1) return TASKS_STATE;
    if ((error = stack_range(stack)) != TASKS_OK) return error;

    unsigned free_slot = TASKS_MAX;
    for (unsigned i = 0; i < TASKS_MAX; ++i) {
        if (control->slots[i].state == TASKS_FREE) {
            if (free_slot == TASKS_MAX) free_slot = i;
        } else if (overlaps(stack, control->slots[i].stack)) {
            return TASKS_OVERLAP;
        }
    }
    if (free_slot == TASKS_MAX) return TASKS_FULL;

    struct tasks_slot *slot = &control->slots[free_slot];
    clear_bytes(slot, sizeof *slot);
    slot->stack = stack;
    slot->entry = entry;
    slot->state = TASKS_READY;
    *id = free_slot;
    return TASKS_OK;
}

enum tasks_error tasks_go(struct tasks_control *control, unsigned *id)
{
    enum tasks_error error = check_control(control);
    if (error != TASKS_OK) return error;
    if (!output_pointer(control, id)) return TASKS_ARGUMENT;
    if (control->current != -1) return TASKS_STATE;
    for (unsigned offset = 0; offset < TASKS_MAX; ++offset) {
        unsigned i = (control->next + offset) % TASKS_MAX;
        if (control->slots[i].state != TASKS_READY) continue;
        if (control->dispatches == UINT_MAX) return TASKS_OVERFLOW;
        control->slots[i].state = TASKS_RUNNING;
        control->current = (int)i;
        control->next = (i + 1u) % TASKS_MAX;
        ++control->dispatches;
        *id = i;
        return TASKS_OK;
    }
    return TASKS_EMPTY;
}

enum tasks_error tasks_mark_yield(struct tasks_control *control)
{
    enum tasks_error error = check_control(control);
    if (error != TASKS_OK) return error;
    if (control->current == -1) return TASKS_STATE;
    struct tasks_slot *slot = &control->slots[(unsigned)control->current];
    if (slot->yields == UINT_MAX) return TASKS_OVERFLOW;
    ++slot->yields;
    slot->state = TASKS_READY;
    control->current = -1;
    return TASKS_OK;
}

enum tasks_error tasks_mark_done(struct tasks_control *control)
{
    enum tasks_error error = check_control(control);
    if (error != TASKS_OK) return error;
    if (control->current == -1) return TASKS_STATE;
    if (control->completed == UINT_MAX) return TASKS_OVERFLOW;
    control->slots[(unsigned)control->current].state = TASKS_DONE;
    control->current = -1;
    ++control->completed;
    return TASKS_OK;
}

enum tasks_error tasks_release(struct tasks_control *control, unsigned id)
{
    enum tasks_error error = check_control(control);
    if (error != TASKS_OK) return error;
    if (id >= TASKS_MAX) return TASKS_ARGUMENT;
    if (control->current != -1 || control->slots[id].state != TASKS_DONE)
        return TASKS_STATE;
    /* Releasing bookkeeping does not zero or free any byte of stack RAM. */
    clear_bytes(&control->slots[id], sizeof control->slots[id]);
    return TASKS_OK;
}

const char *tasks_error_name(enum tasks_error error)
{
    switch (error) {
    case TASKS_OK: return "ok";
    case TASKS_ARGUMENT: return "argument";
    case TASKS_STATE: return "state";
    case TASKS_FULL: return "full";
    case TASKS_EMPTY: return "empty";
    case TASKS_STACK: return "stack";
    case TASKS_ALIGNMENT: return "alignment";
    case TASKS_OVERLAP: return "overlap";
    case TASKS_RANGE: return "range";
    case TASKS_OVERFLOW: return "overflow";
    case TASKS_PLATFORM: return "platform";
    }
    return "unknown";
}

const char *tasks_state_name(enum tasks_state state)
{
    switch (state) {
    case TASKS_FREE: return "FREE";
    case TASKS_READY: return "READY";
    case TASKS_RUNNING: return "RUNNING";
    case TASKS_DONE: return "DONE";
    }
    return "unknown";
}
