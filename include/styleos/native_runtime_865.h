#ifndef STYLEOS_NATIVE_RUNTIME_865_H
#define STYLEOS_NATIVE_RUNTIME_865_H
#include <styleos/runtime_bank_862.h>
#include <styleos/process_api.h>
#include <styleos/window_service.h>
#include <styleos/native_tea_staging.h>
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
/* Launch a pre-authorized canonical .tea, using the existing exact manifest
 * and package/ELF SHA pins. All source bytes and pin metadata are trusted
 * immutable storage for this synchronous call, not user virtual addresses.
 * No package installation, signature verification or filesystem authority.
 * Validate package before acquiring a process lease. Only zero GPR entry. */
enum ps_status s865_native_launch_tea(const uint8_t *,size_t,const struct nts_pin *,
    uint32_t session,const char *title,struct ws_bounds,struct s865_window_launch *);
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
/* Acceptance-only fault injection; requires empty resident registry. */
bool s865_native_probe_window_rollback(const struct rt865_launch_request *,uint32_t);
#endif
/* Trusted provider may admit one isolated shell controller. Exact identity
 * grants only a coalesced redraw request; no framebuffer or kernel pointer.
 * Return 0 from the controller syscall on refusal, nonzero instance epoch
 * on success. Controller exit/fault does not revoke application ownership. */
#define S865_CALL_SHELL_REDRAW 0x104u
#define S865_CALL_SHELL_EVENT 0x105u
#define S865_CALL_SHELL_DECIDE 0x106u
/* Trusted input submits F10 once; controller reads epoch/key and commits
 * an exact epoch-bound decision. Pending input survives uncommitted death. */
bool s865_shell_submit_refresh(void);
/* Only ordinary provisioned controller. Keys: F4/F8; title region
 * 0x101..0x17e (distance from right, saturated at 126); task button
 * 0x200 active, 0x201 inactive; 0x301..0x30c shell menu/shortcut events.
 * No caller-controlled object pointer.
 * Input polling waits for completed command or bounded exhaustion. */
bool s865_shell_submit_command(unsigned key);
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
bool s865_launch_native_sample(void);
#ifdef STYLEOS_LIVE_VOLUME_39
struct po64_row;
/* Explicit Explorer selection of the individually approved package only. */
bool s865_launch_native_file(const struct po64_row *);
#endif
#endif
bool s865_shell_command_waiting(void);
unsigned s865_shell_take_command(void);
uint64_t s865_shell_controller_event(uint32_t,uint64_t,uint64_t);
uint64_t s865_shell_controller_decide(uint32_t,uint64_t,uint64_t,uint64_t,uint64_t);
uint64_t s865_shell_take_refresh(void);
enum ps_status s865_shell_controller_launch(const struct rt865_launch_request *,uint32_t *);
/* Install a copied, trusted immutable source capability before submitting
 * a refresh. Source reader and opaque storage must outlive this registration.
 * Bounded recovery operates in dispatch opportunities, not wall time. */
enum ps_status s865_shell_configure_recovery(const struct rt865_launch_request *);
bool s865_shell_recovery_busy(void);
bool s865_shell_recovery_exhausted(void);
uint64_t s865_shell_controller_request(uint32_t,uint64_t,uint64_t);
uint64_t s865_shell_take_redraw(void);
void s865_native_pump(void);
/* Kernel-owned execution loop, independent of the bounded desktop callback.
 * A returned step must leave the safe root and interrupt contract intact.
 * This is a cooperative boundary, not isolation of privileged shell faults. */
_Noreturn void s865_native_supervise(void (*shell_step)(void *),void *);
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
void s865_native_request_shell_gap(void);
bool s865_native_probe_shell_controller(void);
bool s865_native_shell_probe_available(void);
#endif
/* Trusted trap continuation on safe root, with exact READY caller identity.
 * Copy only from the admitted private data page, never dereference user VA.
 * On failure leave kernel destination unchanged. Destination is trusted
 * separate kernel scratch, not caller controlled. */
bool s865_native_copy_data(unsigned,uint32_t,uint64_t,uint64_t,uint64_t,void *,size_t);
/* Trusted compositor release between two visible native windows. */
void native_drop_gesture(uint32_t source_window,uint32_t target_window);
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
