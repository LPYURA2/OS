#ifndef STYLEOS_PROCESS_TRAP_847_H
#define STYLEOS_PROCESS_TRAP_847_H
#include <styleos/user_root_846.h>
#include <styleos/user_processes.h>
/* Zero-initialized supervisor-owned control before first init.
 * Supervisor-only model. The adapter must establish real CR3, CS, SS,
 * IST address, interrupt masking, immutable executable code and table audit.
 * No privileged instruction, physical page release or raw user dereference. */
enum s847_state { S847_READY=1,S847_RUNNING,S847_FAULTED,S847_EXITED };
enum s847_action { S847_RETURN=1,S847_YIELD,S847_EXIT,S847_FAULT };
enum { S847_CALL_YIELD=1,S847_CALL_EXIT=2,S847_CALL_RANGE=3 };
struct s847_slot { uint64_t root; struct up_context context; enum s847_state state; };
struct s847_control {
 struct s847_slot slot[2]; uint64_t safe,cs,ss; int current; unsigned next; bool ready;
};
struct s847_decision { enum s847_action action; uint64_t result; };
bool s847_init(struct s847_control *,uint64_t safe,uint64_t a,uint64_t b,uint16_t cs,uint16_t ss);
bool s847_dispatch(struct s847_control *,uint64_t live_root,unsigned *,struct up_context *);
/* Called with authenticated hardware state while on the active child root.
 * Non-return actions MUST return via safe root before next dispatch or cleanup.
 * range syscall returns only a status, never copies bytes. */
bool s847_trap(struct s847_control *,const struct up_context *,const struct up_event *,struct s847_decision *);
#endif
