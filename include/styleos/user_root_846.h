#ifndef STYLEOS_USER_ROOT_846_H
#define STYLEOS_USER_ROOT_846_H
#include <styleos/apic_root_842.h>
#include <styleos/physical_arena_844.h>
#define S846_IST UINT64_C(0x400000003000)
struct s846_guard { uint64_t pa[3]; };
#define S846_CODE UINT64_C(0x10000000)
#define S846_DATA UINT64_C(0x12000000)
#define S846_STACK UINT64_C(0x11000000)
#define S846_OWNER_A UINT32_C(0x8461)
#define S846_OWNER_B UINT32_C(0x8462)
#ifdef STYLEOS_KERNEL_CAPACITY_32
#if !(defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
#error expanded capacity requires generation-bound root metadata
#endif
#define S846_ROOT_COUNT 32u
#elif (defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
#define S846_ROOT_COUNT 4u
#else
#define S846_ROOT_COUNT 2u
#endif
/* Trusted detached layout. A zero triple denotes a vacant slot in the
 * expanded sparse API. Fixed VAs and physical order code/data/stack. */
struct s846_extent { unsigned code,data,stack; };
/* Three contiguous pages per process: code, data, stack. Both leases must
 * be live, distinct, and owned by their respective process. Caller has
 * admitted the scratch table storage and serialized against allocation.
 * This module never activates a root or executes user instructions. */
struct s846_root { struct pg_plan plan; struct pg_result built; bool ready;
#if (defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
 struct s844_lease lease;
#endif
};
bool s846_build(const struct s842_root *,const struct s842_cache *,
 const struct s844_arena *,const struct s844_lease [S846_ROOT_COUNT],unsigned id,const struct s846_guard *,
 uint64_t (*tables)[512],uint64_t tables_pa,struct s846_root *);
bool s846_audit(const struct s842_root *,const struct s842_cache *,
 const struct s844_arena *,const struct s844_lease [S846_ROOT_COUNT],unsigned id,const struct s846_guard *,const struct s846_root *);
/* Sparse admission: a zero lease denotes a vacant slot. All allocated
 * arena pages must be represented by live leases. The caller serializes
 * safe-root operations and separately proves nonoverlap of root tables.
 * Historical full-set APIs below retain their original contract. */
#if (defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
bool s865_root_build(const struct s842_root *,const struct s842_cache *,const struct s844_arena *,
 const struct s844_lease [S846_ROOT_COUNT],unsigned,const struct s846_guard *,uint64_t (*)[512],uint64_t,struct s846_root *);
bool s865_root_audit(const struct s842_root *,const struct s842_cache *,const struct s844_arena *,
 const struct s844_lease [S846_ROOT_COUNT],unsigned,const struct s846_guard *,const struct s846_root *);
/* Expanded detached roots only. Every live lease has its exact extent;
 * all arena pages must be accounted for. Existing APIs remain three-page.
 * Caller proves table ownership and disjointness across all child roots. */
bool s875_root_build(const struct s842_root *,const struct s842_cache *,const struct s844_arena *,
 const struct s844_lease [S846_ROOT_COUNT],const struct s846_extent [S846_ROOT_COUNT],
 unsigned,const struct s846_guard *,uint64_t (*)[512],uint64_t,struct s846_root *);
bool s875_root_audit(const struct s842_root *,const struct s842_cache *,const struct s844_arena *,
 const struct s844_lease [S846_ROOT_COUNT],const struct s846_extent [S846_ROOT_COUNT],
 unsigned,const struct s846_guard *,const struct s846_root *);
/* Internal safe-root alias calculation. Caller must separately prove live
 * process identity, serialized safe CR3, and full page-table audit.
 * The output is not a user pointer or an authority to copy. */
uint64_t s875_data_pa(const struct s844_arena *,struct s844_lease,
                     struct s846_extent,uint64_t va,size_t bytes);
#endif
/* Verify all four detached roots in the wide host profile. */
#if (defined(STYLEOS_TEST_WIDE_ROOTS_864) || defined(STYLEOS_OWNED_SHELL_865))
bool s846_audit_set(const struct s842_root *,const struct s842_cache *,
 const struct s844_arena *,const struct s844_lease [S846_ROOT_COUNT],
 const struct s846_guard *,const struct s846_root [S846_ROOT_COUNT]);
#else
/* Verify both detached roots together before an eventual transition. */
bool s846_audit_pair(const struct s842_root *,const struct s842_cache *,
 const struct s844_arena *,const struct s844_lease [S846_ROOT_COUNT],const struct s846_guard *,const struct s846_root *,const struct s846_root *);
#endif
#endif
