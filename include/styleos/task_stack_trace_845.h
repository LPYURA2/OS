#ifndef STYLEOS_TASK_STACK_TRACE_845_H
#define STYLEOS_TASK_STACK_TRACE_845_H
#include <styleos/task_stacks_845.h>
/* The two integer-only workloads increment their own stack word before
 * incrementing their global progress counter. An IRQ between the two
 * instructions may see the stack count ahead by exactly one. */
struct s845_trace {
 uint64_t count[2],progress[2],ticks;
};
bool s845_observe(struct s845_trace *,const struct s845_pair *,const struct s843_scheduler *,const struct s843_frame *,uint64_t a,uint64_t b);
#endif
