#ifndef STYLEOS_WX_FAULT_TRIAL_791_H
#define STYLEOS_WX_FAULT_TRIAL_791_H
#include <styleos/paging.h>
/* Terminal negative RX write / NX instruction fetch, no UEFI return. */
__attribute__((noreturn)) void s791_terminal_trial(struct pg_experiment *);
#endif
