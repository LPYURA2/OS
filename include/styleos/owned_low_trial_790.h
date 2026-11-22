#ifndef STYLEOS_OWNED_LOW_TRIAL_790_H
#define STYLEOS_OWNED_LOW_TRIAL_790_H
#include <styleos/paging.h>
/* Terminal QEMU experiment. NEVER return to UEFI after loading TSS. */
__attribute__((noreturn)) void s790_terminal_trial(struct pg_experiment *);
#endif
