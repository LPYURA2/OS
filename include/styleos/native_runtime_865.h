#ifndef STYLEOS_NATIVE_RUNTIME_865_H
#define STYLEOS_NATIVE_RUNTIME_865_H
#include <styleos/runtime_bank_862.h>
#include <styleos/process_api.h>
/* Trusted kernel-side application source, not a syscall. Caller must
 * authorize the immutable reader/digest and own request/output storage.
 * No auto-run of arbitrary disk data, no session/window grant.
 * Current native ABI: one code page, one data page, one stack page. */
#ifdef STYLEOS_OWNED_SHELL_865
enum ps_status s865_native_launch(const struct rt865_launch_request *,uint32_t *);
void s865_native_pump(void);
/* Read-only snapshot for a trusted supervisor on the owned root, IF=0.
 * No CPL3 entry. CPU ticks are accepted LAPIC preemptions, not elapsed time;
 * resident_bytes counts the three leased user pages, not total system memory. */
enum pa_status s865_native_snapshot(const struct pa_request *,struct pa_row *,size_t,
                                   struct pa_reply *);
/* Trusted supervisor authenticates request, not a user syscall. */
enum ps_status s865_native_stop(uint32_t pid,uint64_t generation,bool authorized);
/* Trusted local supervisor only; resolve the never-reused PID to current
 * generation after validating idle/root and target policy. No user syscall. */
enum ps_status s865_native_stop_selected(uint32_t pid);
#endif
#endif
