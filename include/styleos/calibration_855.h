#ifndef STYLEOS_CALIBRATION_855_H
#define STYLEOS_CALIBRATION_855_H
#include <styleos/monotonic_855.h>
/* Pure single-counter calibration model. Caller must independently attest
 * reference rate, tick bounds and actual counter brackets. No hardware I/O. */
#define S855_PIT_NOMINAL_HZ UINT64_C(1193182)
#define S855_CAL_SAMPLES 3u
struct calibration_window_855 {
    uint64_t start_before,start_after,end_before,end_after;
    uint32_t ref_ticks_min,ref_ticks_max;
};
struct calibration_result_855 { uint64_t min_hz,max_hz,estimate_hz; };
bool calibration_pit_855(const struct calibration_window_855 *windows, unsigned count,
                         struct calibration_result_855 *out);
#endif
