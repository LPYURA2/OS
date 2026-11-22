#ifndef STYLEOS_GUARD_PF_TRIAL_787_H
#define STYLEOS_GUARD_PF_TRIAL_787_H
#include <styleos/paging.h>
/* Isolated terminal genuine #BP→unmapped-guard #PF on IST2. Never Desktop. */
#ifdef STYLEOS_TEST_GUARD_PF_787
__attribute__((noreturn)) void s787_terminal_trial(struct pg_experiment *p);
#endif
#endif
