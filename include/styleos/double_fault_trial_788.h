#ifndef STYLEOS_DOUBLE_FAULT_TRIAL_788_H
#define STYLEOS_DOUBLE_FAULT_TRIAL_788_H
#include <styleos/paging.h>
/* Terminal QEMU double-fault delivery via IST3, never the ordinary Desktop. */
#ifdef STYLEOS_TEST_DOUBLE_FAULT_788
__attribute__((noreturn)) void s788_terminal_trial(struct pg_experiment *p);
#endif
#endif
