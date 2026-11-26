#ifndef STYLEOS_FX_GUEST_849_H
#define STYLEOS_FX_GUEST_849_H
#include <styleos/user_root_846.h>
#include <styleos/fx_context_849.h>
#ifdef STYLEOS_TEST_FOUR_GUEST_864
bool s849_guest_init(const struct s846_root child[S846_ROOT_COUNT],const struct s844_lease leases[S846_ROOT_COUNT],uint64_t safe);
#else
bool s849_guest_init(const struct s846_root child[2],uint64_t safe);
#endif
#ifdef STYLEOS_TEST_RUNTIME_RECYCLE_862
bool s849_guest_bind_leases(const struct s844_lease [2],bool reused);
#endif
bool s849_guest_dispatch(unsigned id,uint64_t root);
bool s849_guest_suspend(unsigned id,uint64_t root,bool terminal);
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
bool s865_fx_retire(unsigned,uint64_t,uint64_t,uint64_t);
#endif
bool s849_guest_finish(uint64_t safe);
#endif
