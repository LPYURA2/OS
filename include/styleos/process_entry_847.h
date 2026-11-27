#ifndef STYLEOS_PROCESS_ENTRY_847_H
#define STYLEOS_PROCESS_ENTRY_847_H
#include <styleos/process_trap_847.h>
/* Same 176-byte frame as T-P1.7; all 15 GPRs in up_context order. */
struct s847_raw { uint64_t gpr[15],vector,error,rip,cs,rflags,rsp,ss; };
_Static_assert(sizeof(struct s847_raw)==176,"raw trap extent");
_Static_assert(offsetof(struct s847_raw,vector)==120,"vector offset");
_Static_assert(offsetof(struct s847_raw,rip)==136,"rip offset");
_Static_assert(offsetof(struct s847_raw,cs)==144,"cs offset");
struct s847_selectors { uint16_t cs,ss,limit; };
/* Pure in-place extension of an admitted 4KiB GDT page, never LGDT/LTR.
 * Caller verifies writable mapping, live GDTR/TR/CS, IF=0 and TSS base.
 * Refusal leaves both GDT and output unchanged. */
bool s847_gdt_append(uint8_t *,size_t,uint16_t limit,uint16_t tr,uint16_t kernel_cs,struct s847_selectors *);
/* Decode only a complete user-origin frame at exact TSS IST top minus 176.
 * Hardware adapter supplies observed frame VA, CR2 and CR3, never user input. */
bool s847_decode(const struct s847_raw *,uint64_t frame_va,uint64_t cr2,uint64_t cr3,uint16_t cs,uint16_t ss,struct up_context *,struct up_event *);
/* Trusted exact mapped extent, not a user-supplied limit. */
bool s875_decode(const struct s847_raw *,uint64_t,uint64_t,uint64_t,uint16_t,uint16_t,
 struct s846_extent,struct up_context *,struct up_event *);
#if defined(STYLEOS_INTEGRATED_RUNTIME_864) || defined(STYLEOS_TEST_RUNTIME_ENTRY_CLASSIFIER_865)
/* Classify the admitted ABI's frame independently of workload instructions,
 * expected GPR patterns, progress words and fixture tick budgets.
 * Caller authenticates actual root/owner/IST/FP and IRQ/EOI. This does not
 * admit a task or a hardware resource. Non-PF CR2 must be normalized to 0. */
enum rt865_frame_kind { RT865_INVALID, RT865_TICK, RT865_SYSCALL, RT865_TERMINAL, RT865_USER_FAULT };
enum rt865_frame_kind rt865_classify_frame(const struct s847_raw *,uint64_t,uint64_t,
 uint64_t,uint64_t,uint16_t,uint16_t,bool);
/* Native executor: authenticated trap with an invalid user RSP is terminal.
 * The strict classifier above remains the resumable-context contract. */
enum rt865_frame_kind rt865_classify_native_frame(const struct s847_raw *,uint64_t,uint64_t,
 uint64_t,uint64_t,uint16_t,uint16_t,bool);
enum rt865_frame_kind s875_classify_native_frame(const struct s847_raw *,uint64_t,uint64_t,
 uint64_t,uint64_t,uint16_t,uint16_t,bool,struct s846_extent);
#endif
/* Bridge a validated raw trap to the lifecycle model. Return true with
 * a decision only on success; modify only RAX for returning calls. The
 * eventual hardware adapter must prove pointer identity and live registers. */
bool s847_apply_raw(struct s847_control *,struct s847_raw *,uint64_t frame_va,uint64_t cr2,uint64_t cr3,struct s847_decision *);
/* Isolated adapter prototype. Invoke only after admitted GDT/IDT/TSS, root
 * audit, IF=0, PCID/PGE disabled, and authenticated dispatcher control.
 * Returns under the original root and stack on non-return traps. */
void s847_enter(const struct up_context *,uint64_t child_root,uint64_t safe_root);
void s847_pf_entry(void);
void s847_call_entry(void);
/* Implemented by eventual hardware adapter, not linked in this draft:
 * 1: IRETQ using raw frame (may change RAX only); 0: return to safe; other: fatal. */
unsigned s847_dispatch_raw(struct s847_raw *,uint64_t cr2,uint64_t cr3);
#endif
