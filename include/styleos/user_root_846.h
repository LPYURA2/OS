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
#ifdef STYLEOS_TEST_WIDE_ROOTS_864
#define S846_ROOT_COUNT 4u
#else
#define S846_ROOT_COUNT 2u
#endif
/* Three contiguous pages per process: code, data, stack. Both leases must
 * be live, distinct, and owned by their respective process. Caller has
 * admitted the scratch table storage and serialized against allocation.
 * This module never activates a root or executes user instructions. */
struct s846_root { struct pg_plan plan; struct pg_result built; bool ready; };
bool s846_build(const struct s842_root *,const struct s842_cache *,
 const struct s844_arena *,const struct s844_lease [S846_ROOT_COUNT],unsigned id,const struct s846_guard *,
 uint64_t (*tables)[512],uint64_t tables_pa,struct s846_root *);
bool s846_audit(const struct s842_root *,const struct s842_cache *,
 const struct s844_arena *,const struct s844_lease [S846_ROOT_COUNT],unsigned id,const struct s846_guard *,const struct s846_root *);
/* Verify all four detached roots in the wide host profile. */
#ifdef STYLEOS_TEST_WIDE_ROOTS_864
bool s846_audit_set(const struct s842_root *,const struct s842_cache *,
 const struct s844_arena *,const struct s844_lease [S846_ROOT_COUNT],
 const struct s846_guard *,const struct s846_root [S846_ROOT_COUNT]);
#else
/* Verify both detached roots together before an eventual transition. */
bool s846_audit_pair(const struct s842_root *,const struct s842_cache *,
 const struct s844_arena *,const struct s844_lease [S846_ROOT_COUNT],const struct s846_guard *,const struct s846_root *,const struct s846_root *);
#endif
#endif
