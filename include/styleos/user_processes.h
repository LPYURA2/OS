#ifndef STYLEOS_USER_PROCESSES_H
#define STYLEOS_USER_PROCESSES_H
#include "styleos/boot_memory.h"

#define UP_SLOTS 2u
#define UP_ROOTS 3u
#define UP_ROOT_SAFE 0u
#define UP_ROOT_A 1u
#define UP_ROOT_B 2u
#define UP_PAGE 4096u
#define UP_TABLE_PAGES 32u
#define UP_CONTROL_BYTES 16384u
#define UP_ENTRY_STACK_BYTES 16384u
#define UP_ARCH_CONTROL_OFFSET 12288u
#define UP_ARCH_CONTROL_BYTES 4096u
#define UP_CODE_VA UINT64_C(0x10000000)
#define UP_STACK_A_VA UINT64_C(0x11000000)
#define UP_STACK_B_VA UINT64_C(0x11010000)
#define UP_DATA_A_VA UINT64_C(0x12000000)
#define UP_DATA_B_VA UINT64_C(0x12010000)
#define UP_DATA_BEFORE UINT64_C(0x123456789abcdef0)
#define UP_DATA_AFTER UINT64_C(0xfedcba9876543210)
#define UP_DISPATCH_COOKIE UINT64_C(0xd15ca11c12345678)

/* Actual exception-observed integer state, NOT the T-P1.6 call context.
 * ARM gpr[0..30]=X0..X30. x86 gpr[0..14]=RAX,RBX,RCX,RDX,RSI,RDI,
 * RBP,R8,R9,R10,R11,R12,R13,R14,R15; gpr[15..30]=0.
 * cs/ss are actual x86 hardware selectors, zero for ARM. */
struct up_context { uint64_t gpr[31], sp, pc, status, cs, ss; };
_Static_assert(offsetof(struct up_context,sp)==248,"user SP offset");
_Static_assert(offsetof(struct up_context,pc)==256,"user PC offset");
_Static_assert(offsetof(struct up_context,status)==264,"user status offset");
_Static_assert(sizeof(struct up_context)==288,"user context extent");
enum up_kind { UP_EVENT_NONE, UP_YIELD=1, UP_EXIT=2, UP_FAULT=3 };
enum up_state { UP_FREE, UP_READY, UP_RUNNING, UP_FAULTED, UP_EXITED };
enum up_error { UP_OK, UP_ARGUMENT, UP_STATE, UP_RANGE, UP_ALIGNMENT,
    UP_OVERLAP, UP_FULL, UP_EMPTY, UP_OVERFLOW, UP_PLATFORM, UP_ORIGIN,
    UP_OPERATION, UP_CONTEXT, UP_MAPPING };
struct up_event {
    enum up_kind kind;
    uint64_t vector, syndrome, fault_va, active_root, kernel_sp;
};
/* Architecture interpretation: x86 mode=CR0, control0=CR4, control1=EFER,
 * control2=PAT, control3=CS; ARM mode=SCTLR, control0=TCR,control1=MAIR,
 * control2=CurrentEL,control3=DAIF. root is actual CR3/TTBR0. */
struct up_machine {
    uint64_t root, mode, control0, control1, control2, control3;
    uint64_t vectors, gdt, tss;
};
struct up_platform {
    uint64_t roots[UP_ROOTS];
    unsigned table_pages[UP_ROOTS];
    uint64_t entry[UP_SLOTS], yield_pc[UP_SLOTS], resume_pc[UP_SLOTS];
    uint64_t exit_pc[UP_SLOTS], fault_pc[UP_SLOTS];
    struct up_machine safe;
};
struct up_spec {
    uint64_t root, entry, code_va, data_va;
    struct boot_range stack_va;
};
struct up_slot {
    struct up_spec spec;
    struct up_context context;
    enum up_state state;
    unsigned yields;
};
struct up_control {
    struct up_slot slots[UP_SLOTS];
    uint64_t safe_root;
    int current;
    unsigned next, dispatches, phase, released;
};
struct up_demo {
    struct up_control control;
    struct up_platform platform;
    const struct boot_memory_evidence *evidence;
    struct up_context before_b;
    uint64_t b_before, b_after;
};
_Static_assert(sizeof(struct up_demo)<=UP_ARCH_CONTROL_OFFSET,"common/arch control disjoint");
/* Final4096bytes are opaque arch scratch, admitted as part of control. */
union up_demo_control { struct up_demo demo; unsigned char bytes[UP_CONTROL_BYTES]; };
extern _Alignas(4096) uint64_t styleos_user_tables[UP_ROOTS][UP_TABLE_PAGES][512];
extern _Alignas(4096) union up_demo_control styleos_user_control;
extern _Alignas(4096) unsigned char styleos_user_entry_stack[UP_ENTRY_STACK_BYTES];
extern _Alignas(4096) unsigned char styleos_user_stack_a[UP_PAGE],styleos_user_stack_b[UP_PAGE];
extern _Alignas(4096) uint64_t styleos_user_data_a[UP_PAGE/8],styleos_user_data_b[UP_PAGE/8];
extern const unsigned char styleos_user_code_a[],styleos_user_code_b[];

void up_init(struct up_control *,uint64_t safe_root);
enum up_error up_create(struct up_control *,const struct up_spec *,unsigned *id);
enum up_error up_go(struct up_control *,unsigned *id);
enum up_error up_accept(struct up_control *,enum up_kind,uint64_t observed_safe_root);
enum up_error up_release(struct up_control *,unsigned id,uint64_t observed_safe_root);
const char *up_error_name(enum up_error);
const char *up_state_name(enum up_state);
/* prepare must prove old platform access and every arch-local mutable object
 * before writing it, then build three complete roots and activate SAFE only.
 * run saves original dispatcher call context, enters actual CPL3/EL0, captures
 * the full hardware exception context and returns under SAFE on original SP.
 * Unexpected privileged exceptions are terminal diagnostics, never UP_FAULT. */
enum up_error up_arch_prepare(const struct boot_memory_evidence *,struct up_platform *);
enum up_error up_arch_run(unsigned id,struct up_context *,struct up_event *);
enum up_error up_arch_observe(struct up_machine *);
_Noreturn void up_fail(const char *);
void up_hex(uint64_t);
void up_field(const char *,uint64_t);
void up_range_line(const char *,struct boot_range);
void up_demo_run(const struct boot_memory_evidence *);
#endif
