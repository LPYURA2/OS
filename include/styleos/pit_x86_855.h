#ifndef STYLEOS_PIT_X86_855_H
#define STYLEOS_PIT_X86_855_H
#include <styleos/pit_capture_855.h>
/* Terminal, single-vCPU Q35/TCG only. The caller must independently prove
 * ownership of PIT2 and port 61, and justify the reference tick bounds. */
struct pit_x86_contract_855 {
    uint64_t root;
    bool terminal, exclusive, post_ebs, reference_attested;
};
struct pit_x86_855 {
    struct pit_capture_855 capture;
    uint64_t root;
    bool bound;
};
bool pit_x86_gate_855(const struct pit_x86_contract_855 *,uint64_t,uint64_t,uint16_t,bool,bool,bool);
bool pit_x86_bind_855(struct pit_x86_855 *,const struct pit_x86_contract_855 *);
bool pit_x86_window_855(struct pit_x86_855 *,const struct pit_capture_contract_855 *,struct calibration_window_855 *);
bool pit_x86_counter_855(struct pit_x86_855 *,uint64_t *);
#endif
