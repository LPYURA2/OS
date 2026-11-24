#ifndef STYLEOS_PIT_CAPTURE_855_H
#define STYLEOS_PIT_CAPTURE_855_H
#include <styleos/calibration_855.h>
/* Exclusive terminal fixture only. Backend attests PIT mode 0, gate low
 * with OUT low after preparation, and reference tick bounds. No claim of
 * hardware restoration: stop must leave gate/speaker disabled. */
#define S855_CAPTURE_MAX_POLLS 2000000u
struct pit_capture_ops_855 {
    bool (*prepare)(void *, uint16_t count);
    bool (*counter)(void *, uint64_t *);
    bool (*gate_on)(void *);
    bool (*output)(void *, bool *);
    bool (*stop)(void *);
};
struct pit_capture_contract_855 {
    uint16_t count;
    uint32_t ref_ticks_min, ref_ticks_max, max_polls;
    bool terminal, exclusive, reference_attested;
};
/* Single caller, no concurrent I/O. Zero-initialized; cleanup failure is
 * sticky. No output publication on any failure, including cleanup. */
struct pit_capture_855 { bool busy, quarantined; };
bool pit_capture_window_855(struct pit_capture_855 *, const struct pit_capture_ops_855 *, void *, const struct pit_capture_contract_855 *, struct calibration_window_855 *);
#endif
