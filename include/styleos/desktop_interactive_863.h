#ifndef STYLEOS_DESKTOP_INTERACTIVE_863_H
#define STYLEOS_DESKTOP_INTERACTIVE_863_H
#include <styleos/desktop_v0.h>
/* Q35/TCG only; no general device admission. */
#ifdef STYLEOS_TEST_FOUR_GUEST_864
#define S863_DISPATCHES_PER_BATCH 26u
#define S863_TASKS_PER_BATCH 4u
#elif defined(STYLEOS_TEST_RUNTIME_MIXED_863)
#define S863_DISPATCHES_PER_BATCH 12u
#define S863_TASKS_PER_BATCH 2u
#else
#define S863_DISPATCHES_PER_BATCH 10u
#define S863_TASKS_PER_BATCH 2u
#endif
#define S863_POLLS_PER_BATCH (S863_DISPATCHES_PER_BATCH+1u)
#if defined(STYLEOS_TEST_FOUR_GUEST_864) && (defined(STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW) || defined(STYLEOS_INTEGRATED_RUNTIME_864))
/* Leave headroom for all per-batch 32-bit counters (including registry
 * transitions), rather than limiting the live service to four launches. */
#define S863_LAUNCH_LIMIT (UINT32_MAX/128u-1u)
#elif defined(STYLEOS_TEST_FOUR_GUEST_864)
#define S863_LAUNCH_LIMIT 4u
#elif defined(STYLEOS_TEST_DESKTOP_AUTONOMOUS_863)
/* Keep all dispatch/poll/identity increments below UINT32_MAX.
 * Refuse rather than wrap. This is not a per-boot workload quota. */
#define S863_LAUNCH_LIMIT (UINT32_MAX/S863_POLLS_PER_BATCH-1u)
#else
#define S863_LAUNCH_LIMIT 2u
#endif
void s863_shell_guard(void);
#ifdef STYLEOS_OWNED_SHELL_865
void s865_native_pump(void);
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
bool s865_native_catalog_request(void);
#endif
#endif
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
void s863_shell_pump(void);
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
bool s863_request_tasks(void);
#endif
#endif
void s863_shell_observe(const struct desk_event *,unsigned,unsigned,unsigned,bool,unsigned,size_t,uint64_t,bool,unsigned,size_t,unsigned,unsigned);
#endif
