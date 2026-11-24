#ifndef STYLEOS_MONOTONIC_855_H
#define STYLEOS_MONOTONIC_855_H
#include <stdbool.h>
#include <stdint.h>
/* Single-writer arithmetic model, no clock source or device ownership implied.
 * Initialize a zeroed object once with a separately attested fixed frequency.
 * Counter wrap, backwards movement or unrepresentable time quarantine the
 * source; last good timestamp remains available for diagnostics only. */
#define S855_NS_PER_SECOND UINT64_C(1000000000)
#define S855_MAX_HZ UINT64_C(10000000000)
struct monotonic_855 {
    uint64_t hz, origin, last_counter, ns;
    bool ready, faulted;
};
bool monotonic_init_855(struct monotonic_855 *, uint64_t hz, uint64_t counter);
bool monotonic_sample_855(struct monotonic_855 *, uint64_t counter);
#endif
