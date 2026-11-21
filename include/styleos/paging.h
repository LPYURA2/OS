#ifndef STYLEOS_PAGING_H
#define STYLEOS_PAGING_H
#include <stdint.h>
#include <stddef.h>
#include "styleos/boot_memory.h"

#define PG_PAGE 4096u
#define PG_TABLE_PAGES 32u
#define PG_TABLE_BYTES (PG_PAGE * PG_TABLE_PAGES)
#define PG_CONTROL_BYTES 4096u
#define PG_DATA_BYTES 4096u
#define PG_MAX_MAPS 16u
#define PG_ALIAS UINT64_C(0x0000400000000000)
#define PG_PROBE (PG_ALIAS + PG_PAGE)
#define PG_INITIAL UINT64_C(0x123456789abcdef0)
#define PG_WRITTEN UINT64_C(0xfedcba9876543210)

enum pg_arch { PG_X86 = 1, PG_ARM = 2 };
enum pg_flags { PG_WRITE = 1, PG_EXEC = 2, PG_DEVICE = 4 };
enum pg_error { PG_OK, PG_ARGUMENT, PG_RANGE, PG_ALIGNMENT, PG_OVERFLOW,
    PG_CONFLICT, PG_CAPACITY, PG_WIDTH, PG_RIGHTS, PG_PROBE_MAPPED,
    PG_CONTEXT, PG_PLATFORM };
struct pg_mapping { uint64_t va, pa, bytes; unsigned flags; };
struct pg_plan {
    enum pg_arch arch;
    unsigned pa_bits;
    uint64_t (*tables)[512]; /* Known image-owned mapped storage, not arbitrary PA. */
    uint64_t tables_pa;
    size_t capacity;
    struct pg_mapping maps[PG_MAX_MAPS];
    size_t count;
    uint64_t probe;
};
struct pg_result { uint64_t root; size_t used; enum pg_error error; };
struct pg_lookup { uint64_t pa; unsigned flags; };
struct pg_result pg_build(const struct pg_plan *);
enum pg_error pg_lookup(const struct pg_plan *, uint64_t va, struct pg_lookup *);
const char *pg_error_name(enum pg_error);
struct pg_storage { struct boot_range tables, control, data; };
enum pg_error pg_admit_layout(const struct boot_memory_evidence *,
    const struct pg_storage *, struct boot_range pool, struct boot_range metadata);

/* Only compiled into new experimental profiles. All three objects are disjoint
 * from the boot pool; active tables are never returned to an allocator. */
extern uint64_t styleos_paging_tables[PG_TABLE_PAGES][512];
extern uint64_t styleos_paging_data[PG_DATA_BYTES / sizeof(uint64_t)];
struct pg_experiment {
    struct pg_plan plan;
    struct pg_result result;
    const struct boot_memory_evidence *evidence;
    uint64_t data_pa;
    uint64_t old_root, before_mode, after_mode;
};
union pg_control {
    struct pg_experiment state;
    unsigned char bytes[PG_CONTROL_BYTES];
};
extern union pg_control styleos_paging_control;
void paging_hex(uint64_t);
void paging_range(const char *, uint64_t, uint64_t);
_Noreturn void paging_fail(const char *);
void paging_experiment(const struct boot_memory_evidence *);
/* Prepare validates platform configuration and adds only critical mappings.
 * Activate consumes a completely validated result, then reads hardware state. */
enum pg_error arch_paging_prepare(struct pg_experiment *);
enum pg_error arch_paging_activate(struct pg_experiment *);
#ifdef STYLEOS_TEST_FRAME_GRANTS
/* Actual inherited CR3 walk; image-owned, WB, present, writable identity only.
 * No guest-owned page mapping or general physical-memory allocator. */
enum pg_error arch_paging_inventory_inherited(struct pg_experiment *);
enum pg_error arch_paging_attest_image_writable(struct pg_experiment *,
                                                uint64_t va, uint64_t bytes);
#endif
/* Dedicated x86-only hardware test: restore a pre-verified inherited CR3.
 * This is not a general kernel root switcher. */
#if defined(STYLEOS_TEST_PAGING_ROUNDTRIP) || defined(STYLEOS_TEST_FRAME_GRANTS)
enum pg_error arch_paging_restore(struct pg_experiment *);
#endif
void arch_paging_fault(uint64_t probe);
#endif
