#ifndef STYLEOS_TASK_STACKS_845_H
#define STYLEOS_TASK_STACKS_845_H
#include <styleos/physical_arena_844.h>
#include <styleos/timer_context_843.h>
#define S845_OWNER_A UINT32_C(0x8451)
#define S845_OWNER_B UINT32_C(0x8452)
struct s845_pair { struct s844_lease lease[2]; bool live; };
struct s845_code { struct boot_range span; uint64_t entry; };
/* Terminal, single-CPU integration contract. All metadata starts zeroed.
 * Caller has already established the owned CR3, RW/NX/WB arena, RX code
 * and a separate guarded IST. Caller serializes all operations with IF=0.
 * Failure after acquiring leases scrubs/releases them, consuming generations.
 * No claim of production task lifetime or protection against kernel UAF. */
bool s845_prepare(struct s844_arena *,struct s845_pair *,struct s843_scheduler *,
 const struct s845_code code[2],struct boot_range ist,uint64_t root,uint64_t cs,uint64_t ss);
/* Terminal only, never IRET to either task afterward. Caller supplies actual
 * live RSP, CR3, RFLAGS, and IST frame after disabling IRQs; must have stopped
 * timer and made the terminal path irrevocable before freeing the stacks. */
bool s845_release_terminal(struct s844_arena *,struct s845_pair *,struct s843_scheduler *,
 struct s843_frame *,uint64_t live_rsp,uint64_t live_root,uint64_t flags);
#endif
