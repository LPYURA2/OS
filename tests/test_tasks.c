#include "styleos/tasks.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Fabricated numeric intervals are deliberate: bookkeeping must never touch
 * the named stack memory or execute a task's function. */
static const struct boot_range stack_a = {0x10000, 0x14000};
static const struct boot_range stack_b = {0x14000, 0x18000};
static const struct boot_range stack_c = {0x20000, 0x24000};
static unsigned entry_calls;

static void entry_a(void) { ++entry_calls; }
static void entry_b(void) { ++entry_calls; }

static void snapshot(const struct tasks_control *control,
                     unsigned char saved[sizeof(struct tasks_control)])
{
    memcpy(saved, control, sizeof *control);
}

static void unchanged(const struct tasks_control *control,
                      const unsigned char saved[sizeof(struct tasks_control)])
{
    assert(!memcmp(control, saved, sizeof *control));
}

#define REFUSE(control, expected, expression) do { \
    unsigned char before_[sizeof(struct tasks_control)]; \
    snapshot((control), before_); \
    assert((expression) == (expected)); \
    unchanged((control), before_); \
} while (0)

static void create_pair(struct tasks_control *control)
{
    unsigned id = 99;
    tasks_init(control);
    assert(tasks_create(control, entry_a, stack_a, &id) == TASKS_OK && id == 0);
    assert(tasks_create(control, entry_b, stack_b, &id) == TASKS_OK && id == 1);
}

static void initialization_and_arguments(void)
{
    struct tasks_control control, expected;
    memset(&control, 0xa5, sizeof control);
    tasks_init(&control);
    memset(&expected, 0, sizeof expected);
    expected.current = -1;
    assert(!memcmp(&control, &expected, sizeof control));
    tasks_init(0);

    unsigned id = 99;
    assert(tasks_create(0, entry_a, stack_a, &id) == TASKS_ARGUMENT);
    assert(tasks_go(0, &id) == TASKS_ARGUMENT);
    assert(tasks_mark_yield(0) == TASKS_ARGUMENT);
    assert(tasks_mark_done(0) == TASKS_ARGUMENT);
    assert(tasks_release(0, 0) == TASKS_ARGUMENT);
    assert(id == 99);
    REFUSE(&control, TASKS_EMPTY, tasks_go(&control, &id));
    assert(id == 99);
    REFUSE(&control, TASKS_ARGUMENT,
           tasks_create(&control, 0, stack_a, &id));
    REFUSE(&control, TASKS_ARGUMENT,
           tasks_create(&control, entry_a, stack_a, 0));
    REFUSE(&control, TASKS_ARGUMENT, tasks_go(&control, 0));
    REFUSE(&control, TASKS_ARGUMENT,
           tasks_create(&control, entry_a, stack_a, &control.next));
    REFUSE(&control, TASKS_ARGUMENT, tasks_go(&control, &control.completed));
    assert(id == 99);

    /* Invalid aligned extents are rejected before dereferencing their pointer. */
    struct tasks_control *outside = (struct tasks_control *)(uintptr_t)
        (UINTPTR_MAX & ~(uintptr_t)(_Alignof(struct tasks_control) - 1u));
    assert(tasks_go(outside, &id) == TASKS_ARGUMENT);
    tasks_init(outside);
    outside = (struct tasks_control *)(void *)((unsigned char *)&control + 1);
    assert(tasks_go(outside, &id) == TASKS_ARGUMENT);
    tasks_init(outside);
    REFUSE(&control, TASKS_ARGUMENT, tasks_go(&control,
        (unsigned *)(uintptr_t)(UINTPTR_MAX & ~(uintptr_t)3)));
    REFUSE(&control, TASKS_ARGUMENT, tasks_go(&control,
        (unsigned *)(void *)((unsigned char *)&id + 1)));
    assert(id == 99);
}

static void stack_and_capacity_refusals(void)
{
    struct tasks_control control;
    tasks_init(&control);
    unsigned id = 99;
    const struct boot_range bad[] = {
        {0x10000, 0x10000}, {0x14000, 0x10000}, {0x10000, 0x13fff},
        {0x10000, 0x15000}, {0, 0}
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        REFUSE(&control, TASKS_STACK,
               tasks_create(&control, entry_a, bad[i], &id));
    REFUSE(&control, TASKS_ALIGNMENT, tasks_create(&control, entry_a,
        ((struct boot_range){0x10001, 0x14001}), &id));
    REFUSE(&control, TASKS_OVERFLOW, tasks_create(&control, entry_a,
        ((struct boot_range){UINT64_MAX & ~UINT64_C(4095), 0x3000}), &id));
    assert(id == 99);

    assert(tasks_create(&control, entry_a, stack_a, &id) == TASKS_OK && id == 0);
    struct tasks_slot first = control.slots[0];
    id = 99;
    const struct boot_range overlaps[] = {
        {0x10000, 0x14000}, {0xd000, 0x11000}, {0x13000, 0x17000}
    };
    for (size_t i = 0; i < sizeof overlaps / sizeof overlaps[0]; ++i)
        REFUSE(&control, TASKS_OVERLAP,
               tasks_create(&control, entry_b, overlaps[i], &id));
    assert(id == 99 && !memcmp(&first, &control.slots[0], sizeof first));
    assert(tasks_create(&control, entry_b, stack_b, &id) == TASKS_OK && id == 1);
    id = 99;
    REFUSE(&control, TASKS_FULL,
           tasks_create(&control, entry_a, stack_c, &id));
    assert(id == 99);
}

static void transitions_and_reuse(void)
{
    struct tasks_control control;
    create_pair(&control);
    unsigned id = 99;
    REFUSE(&control, TASKS_STATE, tasks_mark_yield(&control));
    REFUSE(&control, TASKS_STATE, tasks_mark_done(&control));
    REFUSE(&control, TASKS_ARGUMENT, tasks_release(&control, TASKS_MAX));
    REFUSE(&control, TASKS_STATE, tasks_release(&control, 0));

    /* Metadata round robin follows the exact six-dispatch hardware demo trace.
     * The host test neither invokes entry functions nor simulates execution. */
    for (unsigned step = 0; step < 6; ++step) {
        unsigned selected = step % TASKS_MAX;
        unsigned other = 1u - selected;
        struct tasks_slot untouched = control.slots[other];
        assert(tasks_go(&control, &id) == TASKS_OK && id == selected);
        assert(control.current == (int)selected);
        assert(control.next == other && control.dispatches == step + 1u);
        assert(control.slots[selected].state == TASKS_RUNNING);
        assert(!memcmp(&untouched, &control.slots[other], sizeof untouched));
        id = 99;
        REFUSE(&control, TASKS_STATE, tasks_go(&control, &id));
        REFUSE(&control, TASKS_STATE,
               tasks_create(&control, entry_a, stack_c, &id));
        REFUSE(&control, TASKS_STATE, tasks_release(&control, selected));
        assert(id == 99);
        if (step < 4) {
            assert(tasks_mark_yield(&control) == TASKS_OK);
            assert(control.slots[selected].state == TASKS_READY);
            assert(control.slots[selected].yields == step / 2u + 1u);
            REFUSE(&control, TASKS_STATE, tasks_mark_yield(&control));
        } else {
            assert(tasks_mark_done(&control) == TASKS_OK);
            assert(control.slots[selected].state == TASKS_DONE);
            assert(control.completed == step - 3u);
            REFUSE(&control, TASKS_STATE, tasks_mark_done(&control));
        }
        assert(control.current == -1);
        assert(!memcmp(&untouched, &control.slots[other], sizeof untouched));
    }
    assert(control.slots[0].yields == 2 && control.slots[1].yields == 2);
    assert(control.completed == 2 && control.dispatches == 6 && !entry_calls);
    id = 99;
    REFUSE(&control, TASKS_EMPTY, tasks_go(&control, &id));
    REFUSE(&control, TASKS_FULL,
           tasks_create(&control, entry_a, stack_c, &id));
    REFUSE(&control, TASKS_OVERLAP,
           tasks_create(&control, entry_a, stack_a, &id));
    assert(id == 99); /* DONE is neither selectable nor reusable yet. */

    struct tasks_slot remaining = control.slots[1];
    /* Context and all slot metadata are cleared by release, not stack RAM. */
    for (unsigned i = 0; i < 12; ++i) control.slots[0].context.gpr[i] = i + 1;
    control.slots[0].context.sp = 0x1234;
    control.slots[0].context.pc = 0x5678;
    assert(tasks_release(&control, 0) == TASKS_OK);
    struct tasks_slot empty;
    memset(&empty, 0, sizeof empty);
    assert(!memcmp(&control.slots[0], &empty, sizeof empty));
    assert(!memcmp(&control.slots[1], &remaining, sizeof remaining));
    REFUSE(&control, TASKS_STATE, tasks_release(&control, 0));
    REFUSE(&control, TASKS_OVERLAP,
           tasks_create(&control, entry_a, stack_b, &id));
    assert(tasks_create(&control, entry_a, stack_a, &id) == TASKS_OK && id == 0);
    assert(control.slots[0].yields == 0 && control.completed == 2);
    assert(!memcmp(&control.slots[1], &remaining, sizeof remaining));
    assert(tasks_go(&control, &id) == TASKS_OK && id == 0);
    REFUSE(&control, TASKS_STATE, tasks_release(&control, 1));
    assert(tasks_mark_done(&control) == TASKS_OK);
    assert(tasks_release(&control, 0) == TASKS_OK);
    assert(tasks_release(&control, 1) == TASKS_OK);
    REFUSE(&control, TASKS_EMPTY, tasks_go(&control, &id));
    assert(!entry_calls);
}

static void rejected_context(struct tasks_control *control)
{
    unsigned id = 99;
    REFUSE(control, TASKS_STATE,
           tasks_create(control, entry_a, stack_c, &id));
    REFUSE(control, TASKS_STATE, tasks_go(control, &id));
    REFUSE(control, TASKS_STATE, tasks_mark_yield(control));
    REFUSE(control, TASKS_STATE, tasks_mark_done(control));
    REFUSE(control, TASKS_STATE, tasks_release(control, 0));
    assert(id == 99);
}

static void malformed_state_and_overflow(void)
{
    struct tasks_control control;
    create_pair(&control);
    control.current = -2; rejected_context(&control);
    create_pair(&control);
    control.current = TASKS_MAX; rejected_context(&control);
    create_pair(&control);
    control.current = 0; rejected_context(&control);
    create_pair(&control);
    control.next = TASKS_MAX; rejected_context(&control);
    create_pair(&control);
    control.slots[1].state = (enum tasks_state)99; rejected_context(&control);
    create_pair(&control);
    control.slots[1].state = TASKS_RUNNING; rejected_context(&control);
    create_pair(&control);
    control.current = 0;
    control.slots[0].state = TASKS_RUNNING;
    control.slots[1].state = TASKS_RUNNING; rejected_context(&control);
    create_pair(&control);
    control.slots[1].entry = 0; rejected_context(&control);
    create_pair(&control);
    control.slots[1].stack.end--; rejected_context(&control);
    create_pair(&control);
    control.slots[1].stack = control.slots[0].stack; rejected_context(&control);
    tasks_init(&control);
    control.slots[1].context.pc = 1; rejected_context(&control);
    tasks_init(&control);
    control.slots[1].yields = 1; rejected_context(&control);

    unsigned id = 99;
    create_pair(&control);
    control.dispatches = UINT_MAX;
    REFUSE(&control, TASKS_OVERFLOW, tasks_go(&control, &id));
    assert(id == 99);
    create_pair(&control);
    assert(tasks_go(&control, &id) == TASKS_OK);
    control.slots[id].yields = UINT_MAX;
    REFUSE(&control, TASKS_OVERFLOW, tasks_mark_yield(&control));
    control.completed = UINT_MAX;
    REFUSE(&control, TASKS_OVERFLOW, tasks_mark_done(&control));
}

static void stack_bytes_are_untouched(void)
{
    _Alignas(4096) static unsigned char bytes[TASKS_STACK_BYTES];
    struct boot_range real = {(uint64_t)(uintptr_t)bytes,
                              (uint64_t)(uintptr_t)bytes + sizeof bytes};
    memset(bytes, 0xa5, sizeof bytes);
    struct tasks_control control;
    tasks_init(&control);
    unsigned id = 99;
    assert(tasks_create(&control, entry_a, real, &id) == TASKS_OK);
    assert(tasks_go(&control, &id) == TASKS_OK);
    assert(tasks_mark_yield(&control) == TASKS_OK);
    assert(tasks_go(&control, &id) == TASKS_OK);
    assert(tasks_mark_done(&control) == TASKS_OK);
    assert(tasks_release(&control, id) == TASKS_OK);
    for (size_t i = 0; i < sizeof bytes; ++i) assert(bytes[i] == 0xa5);
    assert(!entry_calls);
}

int main(void)
{
    initialization_and_arguments();
    stack_and_capacity_refusals();
    transitions_and_reuse();
    malformed_state_and_overflow();
    stack_bytes_are_untouched();
    assert(!strcmp(tasks_error_name(TASKS_OVERFLOW), "overflow"));
    assert(!strcmp(tasks_error_name((enum tasks_error)99), "unknown"));
    assert(!strcmp(tasks_state_name(TASKS_DONE), "DONE"));
    assert(!strcmp(tasks_state_name((enum tasks_state)99), "unknown"));
    puts("Tasks: transitions, empty/full, stacks, refusals, DONE exclusion and reuse PASS");
    return 0;
}
