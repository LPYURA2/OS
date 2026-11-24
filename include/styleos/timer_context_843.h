#ifndef STYLEOS_TIMER_CONTEXT_843_H
#define STYLEOS_TIMER_CONTEXT_843_H
#include <styleos/tasks.h>
#include <stdbool.h>
/* Two supervisor tasks on one admitted root, integer-only test profile.
 * Push order: RAX RCX RDX RBX RBP RSI RDI R8..R15, followed by the
 * five hardware words supplied by a 64-bit interrupt gate using IST. */
struct s843_frame {
 uint64_t r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rbp,rbx,rdx,rcx,rax;
 uint64_t rip,cs,rflags,rsp,ss;
};
_Static_assert(offsetof(struct s843_frame,rip)==120,"IRQ RIP offset");
_Static_assert(offsetof(struct s843_frame,rsp)==144,"IRQ RSP offset");
_Static_assert(sizeof(struct s843_frame)==160,"IRQ frame size");
struct s843_task { struct boot_range code,stack; uint64_t entry; };
struct s843_scheduler {
 struct tasks_control control;
 struct s843_task task[TASKS_MAX];
 struct s843_frame saved[TASKS_MAX];
 struct boot_range ist;
 uint64_t root,cs,ss,ticks;
 bool ready;
};
/* Caller owns the actual root/IST admission and interrupt masking. No
 * instructions modifying CR3, IF, MMIO or device state in this module. */
bool s843_init(struct s843_scheduler *,const struct s843_task [TASKS_MAX],
 struct boot_range ist,uint64_t root,uint64_t cs,uint64_t ss);
bool s843_first(const struct s843_scheduler *,struct s843_frame *);
/* On success, save interrupted registers and replace only the supplied IST
 * frame with the next task's frame. On refusal, neither object is changed. */
bool s843_tick(struct s843_scheduler *,struct s843_frame *,uint64_t live_root);
#endif
