#ifndef STYLEOS_USER_PREEMPT_848_H
#define STYLEOS_USER_PREEMPT_848_H
#include <styleos/process_entry_847.h>
/* Hardware adapter must observe these from actual CPU and LAPIC, with no
 * user input. This module is a model: it neither reads hardware nor sends EOI.
 * Return to SAFE before dispatch and before releasing any physical lease. */
struct s848_irq_evidence { uint64_t root,frame_va,live_flags; uint32_t isr; };
bool s848_user_dispatch(struct s847_control *,uint64_t,unsigned *,struct up_context *);
bool s848_timer_suspend(struct s847_control *,const struct s847_raw *,const struct s848_irq_evidence *);
/* For live INT80/#PF, preserve saved IF and all GPRs except documented RAX
 * syscall return. Same normalized stored-state convention as the IRQ path. */
bool s848_user_trap(struct s847_control *,struct s847_raw *,uint64_t,uint64_t,uint64_t,uint64_t,struct s847_decision *);
#endif
