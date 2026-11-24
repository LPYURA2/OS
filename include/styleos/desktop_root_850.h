#ifndef STYLEOS_DESKTOP_ROOT_850_H
#define STYLEOS_DESKTOP_ROOT_850_H
#include <styleos/desktop_gop_850.h>
#include <styleos/user_root_846.h>
/* Detached composition, never an activation or MMIO access grant. The caller
 * independently attests destination storage and physical correspondence,
 * disjoint metadata, stable boot/cache evidence and device ownership. */
struct s850_desktop_root {
 struct pg_plan plan;
 struct pg_result built;
 struct s850_gop_plan gop;
 bool ready;
};
bool s850_desktop_build(const struct s842_root *,const struct s850_gop_request *,uint64_t (*)[512],uint64_t,struct s850_desktop_root *);
bool s850_desktop_audit(const struct s842_root *,const struct s850_gop_request *,const struct s850_desktop_root *);
/* Same S846 IST virtual layout, independent trusted backing input. Guarded
 * audit must use the same independently attested frame identities. */
bool s850_desktop_build_guarded(const struct s842_root *,const struct s850_gop_request *,const struct s846_guard *,uint64_t (*)[512],uint64_t,struct s850_desktop_root *);
bool s850_desktop_audit_guarded(const struct s842_root *,const struct s850_gop_request *,const struct s846_guard *,const struct s850_desktop_root *);
#endif
