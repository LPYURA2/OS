#ifndef STYLEOS_TEST_GUEST_WORKLOAD_865_H
#define STYLEOS_TEST_GUEST_WORKLOAD_865_H
#include <styleos/native_image_slot_865.h>
#include <styleos/user_root_846.h>
/* Test-only producer. Caller must already own and independently audit
 * every lease and detached root; no admission or CPU authority here. */
struct s865_fixture_image {
 struct nte_loaded image;
 uint64_t loop_end,terminal_pc,call_pc;
};
bool s865_fixture_source(unsigned,struct s865_image_source *);
bool s865_fixture_observe(unsigned,struct s844_lease,const struct nte_loaded *,
 struct s865_fixture_image *);
bool s865_fixture_request_stop(unsigned,uint64_t,const struct s844_lease *,unsigned);
bool s865_fixture_force_timer(unsigned,uint64_t,uint64_t,unsigned);
#endif
