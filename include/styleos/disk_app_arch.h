#ifndef STYLEOS_DISK_APP_ARCH_H
#define STYLEOS_DISK_APP_ARCH_H
#include "styleos/boot_memory.h"
#include "styleos/disk_app.h"

/* T-P1.8B only: finite, image-owned, single-CPU AArch64 resources. */
#define DA_PAGE 4096u
#define DA_ROOTS 2u
#define DA_ROOT_SAFE 0u
#define DA_ROOT_USER 1u
#define DA_TABLE_PAGES 32u
#define DA_CONTROL_BYTES 16384u
#define DA_ARCH_CONTROL_OFFSET 12288u
#define DA_ARCH_CONTROL_BYTES 4096u
#define DA_ENTRY_STACK_BYTES 16384u
#define DA_CODE_VA UINT64_C(0x10000000)
#define DA_STACK_VA UINT64_C(0x11000000)

/* Exception-observed X0..X30, SP_EL0, ELR_EL1 and SPSR_EL1. The final two
 * fields are zero on this architecture; this is not an FP/SIMD context. */
struct da_context { uint64_t gpr[31], sp, pc, status, cs, ss; };
_Static_assert(offsetof(struct da_context,sp)==248,"disk-app SP offset");
_Static_assert(offsetof(struct da_context,pc)==256,"disk-app PC offset");
_Static_assert(offsetof(struct da_context,status)==264,"disk-app status offset");
_Static_assert(sizeof(struct da_context)==288,"disk-app complete integer context");
enum da_event_kind { DA_EVENT_NONE=0, DA_EVENT_EXIT=2, DA_EVENT_FAULT=3 };
struct da_event {
    enum da_event_kind kind;
    uint64_t vector, syndrome, fault_va, active_root, kernel_sp;
};
/* mode=SCTLR; control0=TCR; control1=MAIR; control2=CurrentEL; control3=DAIF.
 * active=UINT64_MAX and context_live=0 establish released arch bookkeeping. */
struct da_machine {
    uint64_t root, mode, control0, control1, control2, control3, vectors;
    uint64_t upper_root, active, dispatcher_sp, context_live;
};
struct da_platform {
    uint64_t roots[DA_ROOTS];
    unsigned table_pages[DA_ROOTS];
    struct da_machine safe;
};
enum da_arch_result {
    DA_ARCH_OK, DA_ARCH_ARGUMENT, DA_ARCH_PLATFORM, DA_ARCH_RANGE,
    DA_ARCH_OVERLAP, DA_ARCH_FULL, DA_ARCH_CONTEXT, DA_ARCH_MAPPING
};

extern _Alignas(4096) uint64_t styleos_disk_app_tables[DA_ROOTS][DA_TABLE_PAGES][512];
extern _Alignas(4096) unsigned char styleos_disk_app_control[DA_CONTROL_BYTES];
extern _Alignas(4096) unsigned char styleos_disk_app_entry_stack[DA_ENTRY_STACK_BYTES];
extern _Alignas(4096) unsigned char styleos_disk_app_code[DA_PAGE];
extern _Alignas(4096) unsigned char styleos_disk_app_user_stack[DA_PAGE];
extern const unsigned char styleos_disk_app_vectors[], styleos_disk_app_vectors_end[];

/* Guest admits every complete resource before writes, validates and copies
 * external code, then proves all read-only I/O/DMA complete before prepare.
 * prepare rechecks the finite architecture closure, establishes instruction
 * visibility, builds SAFE/USER roots and activates SAFE. VirtIO is unmapped.
 * run always enters through ERET. On exception it switches to SAFE, restores
 * the original dispatcher stack, then clears the active context. */
enum da_arch_result da_arch_prepare(const struct boot_memory_evidence *,
                                   const struct da_info *,struct da_platform *);
enum da_arch_result da_arch_run(struct da_context *,struct da_event *);
enum da_arch_result da_arch_observe(struct da_machine *);
void da_arch_dump(const struct da_platform *);
const char *da_arch_error_name(enum da_arch_result);
_Noreturn void da_fail(const char *);
void da_hex(uint64_t);
void da_field(const char *,uint64_t);
void da_range_line(const char *,struct boot_range);
#endif
