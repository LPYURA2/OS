#ifndef STYLEOS_NATIVE_RUNTIME_865_H
#define STYLEOS_NATIVE_RUNTIME_865_H
#include <styleos/runtime_bank_862.h>
/* Trusted kernel-side application source, not a syscall. Caller must
 * authorize the immutable reader/digest and own request/output storage.
 * No auto-run of arbitrary disk data, no session/window grant.
 * Current native ABI: one code page, one data page, one stack page. */
#ifdef STYLEOS_OWNED_SHELL_865
enum ps_status s865_native_launch(const struct rt865_launch_request *,uint32_t *);
void s865_native_pump(void);
/* Trusted supervisor authenticates request, not a user syscall. */
enum ps_status s865_native_stop(uint32_t pid,uint64_t generation,bool authorized);
#endif
#endif
