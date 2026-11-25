#ifndef STYLEOS_RUNTIME_ROOTS_861_H
#define STYLEOS_RUNTIME_ROOTS_861_H
#include <styleos/desktop_root_850.h>
/* Pre-activation check only: caller owns and serializes all input storage,
 * attests its VA/PA correspondence and supplies same-boot inherited evidence.
 * GOP stays absent from both user roots; no additional mapping or grant. */
bool rt861_roots_audit(const struct s842_root *,const struct s850_gop_request *,
 const struct s844_arena *,const struct s844_lease [2],const struct s846_guard *,
 const struct s850_desktop_root *,const struct s846_root *,const struct s846_root *);
#ifdef STYLEOS_TEST_RUNTIME_ROOTS_861
bool rt861_roots_audit_frozen(const struct s842_root *,const struct s842_cache *,const struct s850_gop_plan *,const struct boot_memory_evidence *,const struct s844_arena *,const struct s844_lease [2],const struct s846_guard *,const struct s850_desktop_root *,const struct s846_root *,const struct s846_root *);
#endif
#endif
