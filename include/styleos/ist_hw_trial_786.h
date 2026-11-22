#ifndef STYLEOS_IST_HW_TRIAL_786_H
#define STYLEOS_IST_HW_TRIAL_786_H
#include <styleos/paging.h>
/* Dedicated terminal QEMU-only 786 hardware path; does not return to UEFI. */
#ifdef STYLEOS_TEST_IST_HW_786
__attribute__((noreturn)) void s786_terminal_trial(struct pg_experiment *p);
#endif
#endif
