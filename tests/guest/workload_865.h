#ifndef STYLEOS_TEST_GUEST_WORKLOAD_865_H
#define STYLEOS_TEST_GUEST_WORKLOAD_865_H
#include <styleos/native_tea_elf.h>
#include <styleos/user_root_846.h>
/* Test-only producer. Caller must already own and independently audit
 * every lease and detached root; no admission or CPU authority here. */
struct s865_fixture_image {
 struct nte_loaded image;
 uint64_t loop_end,terminal_pc,call_pc;
};
bool s865_fixture_stage_one(const struct s844_arena *,unsigned,struct s844_lease,
 struct s865_fixture_image *);
bool s865_fixture_request_stop(unsigned,uint64_t,const struct s844_lease *,unsigned);
bool s865_fixture_force_timer(unsigned,uint64_t,uint64_t,unsigned);
#endif
