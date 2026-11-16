#ifndef STYLEOS_TASKS_H
#define STYLEOS_TASKS_H
#include "styleos/boot_memory.h"

#define TASKS_MAX 2u
#define TASKS_STACK_BYTES 16384u
#define TASKS_CONTROL_BYTES 4096u
#define TASKS_GUARD_BYTES 16u
#define TASKS_REG_BASE UINT64_C(0x1122000000000000)
#define TASKS_DISPATCH_COOKIE UINT64_C(0xd15ca11c12345678)
#define TASKS_COOKIE_A UINT64_C(0xa11ce55a12345678)
#define TASKS_COOKIE_B UINT64_C(0xb0b55aa987654321)

/* Experimental integer ABI only. x86 gpr0..7 = RBX,RBP,RDI,RSI,R12..R15;
 * ARM gpr0..11 = X19..X30. Assembly uses fixed SP96/PC104 offsets. */
struct tasks_context { uint64_t gpr[12], sp, pc; };
_Static_assert(offsetof(struct tasks_context,sp)==96,"context SP offset");
_Static_assert(offsetof(struct tasks_context,pc)==104,"context PC offset");
_Static_assert(sizeof(struct tasks_context)==112,"context extent");
typedef void (*tasks_entry)(void);
enum tasks_state { TASKS_FREE, TASKS_READY, TASKS_RUNNING, TASKS_DONE };
enum tasks_error { TASKS_OK, TASKS_ARGUMENT, TASKS_STATE, TASKS_FULL,
    TASKS_EMPTY, TASKS_STACK, TASKS_ALIGNMENT, TASKS_OVERLAP, TASKS_RANGE,
    TASKS_OVERFLOW, TASKS_PLATFORM };
struct tasks_slot {
    _Alignas(16) struct tasks_context context;
    struct boot_range stack;
    tasks_entry entry;
    enum tasks_state state;
    unsigned yields;
};
struct tasks_control {
    struct tasks_slot slots[TASKS_MAX];
    _Alignas(16) struct tasks_context dispatcher;
    int current; /* -1 on the dispatcher */
    unsigned next, completed, dispatches;
};
void tasks_init(struct tasks_control *);
enum tasks_error tasks_create(struct tasks_control *, tasks_entry,
                              struct boot_range full_stack, unsigned *id);
enum tasks_error tasks_go(struct tasks_control *, unsigned *id);
enum tasks_error tasks_mark_yield(struct tasks_control *);
enum tasks_error tasks_mark_done(struct tasks_control *);
enum tasks_error tasks_release(struct tasks_control *, unsigned id);
const char *tasks_error_name(enum tasks_error);
const char *tasks_state_name(enum tasks_state);

struct tasks_arch_state {
    uint64_t mode, root, control0, control1, flags, cs;
    uint64_t idt_base, idt_bytes, gdt_base, gdt_bytes;
    uint64_t pat, pa_bits, gdt_pages, gdt_pa0, gdt_pa1;
};
enum tasks_error tasks_arch_observe(const struct boot_memory_evidence *,
                                   struct tasks_arch_state *);
int tasks_arch_unchanged(const struct tasks_arch_state *,
                         const struct tasks_arch_state *);
void tasks_arch_switch(struct tasks_context *from, const struct tasks_context *to);
void tasks_arch_dispatch(struct tasks_context *from, const struct tasks_context *to);
uint64_t tasks_arch_probe(struct tasks_context *from, const struct tasks_context *to,
                          uint64_t tag);
_Noreturn void tasks_arch_finish(struct tasks_context *from,
                                const struct tasks_context *to);
extern const unsigned char styleos_tasks_probe_resume[], styleos_tasks_exit_resume[];
extern const unsigned char styleos_tasks_trap_return[];
extern const unsigned char styleos_tasks_dispatch_resume[];
_Noreturn void tasks_completed_resumed(void);
_Noreturn void styleos_tasks_bootstrap(void);
void styleos_tasks_a(void);
void styleos_tasks_b(void);
void tasks_demo_run(const struct boot_memory_evidence *);

struct tasks_demo {
    struct tasks_control control;
    const struct boot_memory_evidence *evidence;
    struct tasks_arch_state before, after;
    uint64_t masks[TASKS_MAX], locals[TASKS_MAX];
    unsigned events[TASKS_MAX], returns[TASKS_MAX];
};
union tasks_demo_control {
    struct tasks_demo demo;
    unsigned char bytes[TASKS_CONTROL_BYTES];
};
extern union tasks_demo_control styleos_tasks_control;
extern unsigned char styleos_tasks_stack_a[TASKS_STACK_BYTES];
extern unsigned char styleos_tasks_stack_b[TASKS_STACK_BYTES];
#endif
