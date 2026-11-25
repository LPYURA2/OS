#ifndef STYLEOS_POINTER_CLOCK_863_H
#define STYLEOS_POINTER_CLOCK_863_H
#include <styleos/monotonic_855.h>
/* Single-CPU trusted counter arithmetic. Each bracket must cover an
 * independently observed consecutive RTC second edge. No device I/O. */
struct pc863_edge {uint64_t lo,hi;unsigned second;};
struct pc863_clock {struct monotonic_855 mono;uint64_t min_hz,max_hz;};
bool pc863_init(struct pc863_clock *,const struct pc863_edge [3]);
uint64_t pc863_sample(struct pc863_clock *,uint64_t);
bool pc863_near(const struct pc863_clock *,uint64_t,uint64_t);
#endif
