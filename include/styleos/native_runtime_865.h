#ifndef STYLEOS_NATIVE_RUNTIME_865_H
#define STYLEOS_NATIVE_RUNTIME_865_H
#include <styleos/runtime_bank_862.h>
#include <styleos/process_api.h>
#include <styleos/window_service.h>
/* Trusted kernel-side application source, not a syscall. Caller must
 * authorize the immutable reader/digest and own request/output storage.
 * No auto-run of arbitrary disk data, no session/window grant.
 * Current native ABI: one code page, one data page, one stack page. */
#ifdef STYLEOS_OWNED_SHELL_865
enum ps_status s865_native_launch(const struct rt865_launch_request *,uint32_t *);
/* Trusted, serialized windowed launch on the safe root. Source and session
 * must already be authorized by the supervisor. No user dispatch before
 * process, session, window and channel are established. Outputs unchanged
 * on refusal. No package installation or untrusted caller entry point. */
struct s865_window_launch { uint32_t pid,window; };
enum ps_status s865_native_launch_window(const struct rt865_launch_request *,
    uint32_t session,const char *title,struct ws_bounds,struct s865_window_launch *);
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
/* Acceptance-only fault injection; requires empty resident registry. */
bool s865_native_probe_window_rollback(const struct rt865_launch_request *,uint32_t);
#endif
void s865_native_pump(void);
/* Kernel-owned execution loop, independent of the bounded desktop callback.
 * A returned step must leave the safe root and interrupt contract intact.
 * This is a cooperative boundary, not isolation of privileged shell faults. */
_Noreturn void s865_native_supervise(void (*shell_step)(void *),void *);
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
void s865_native_request_shell_gap(void);
#endif
/* Trusted trap continuation on safe root, with exact READY caller identity.
 * Copy only from the admitted private data page, never dereference user VA.
 * On failure leave kernel destination unchanged. Destination is trusted
 * separate kernel scratch, not caller controlled. */
bool s865_native_copy_data(unsigned,uint32_t,uint64_t,uint64_t,uint64_t,void *,size_t);
/* Trusted, serialized safe-root continuation only. Result is a kernel-only
 * alias of the exact admitted private data span; never return to CPL3.
 * Valid until the next scheduling/admission/release boundary. */
volatile uint8_t *s865_native_data_span(unsigned,uint32_t,uint64_t,uint64_t,uint64_t,size_t);
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
