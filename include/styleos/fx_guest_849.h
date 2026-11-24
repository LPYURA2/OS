#ifndef STYLEOS_FX_GUEST_849_H
#define STYLEOS_FX_GUEST_849_H
#include <styleos/user_root_846.h>
#include <styleos/fx_context_849.h>
bool s849_guest_init(const struct s846_root child[2],uint64_t safe);
bool s849_guest_dispatch(unsigned id,uint64_t root);
bool s849_guest_suspend(unsigned id,uint64_t root,bool terminal);
bool s849_guest_finish(uint64_t safe);
#endif
