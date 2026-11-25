#ifndef STYLEOS_RUNTIME_ENTRY_858_H
#define STYLEOS_RUNTIME_ENTRY_858_H
#include <stdbool.h>
#include <stdint.h>
/* Internal kernel contract, not a user ABI or hardware capability. Expected
 * values originate in the audited S850 plan; observed ones from live CPU. */
struct rt858_expected {
 uint64_t old_root,root,gdt,idt,stack_lo,stack_hi;
 uint16_t gdt_limit,tr,cs;
};
struct rt858_live {
 uint64_t root,gdt,idt,rsp,cr0,cr4,efer,rflags;
 uint16_t gdt_limit,idt_limit,tr,cs;
};
/* Shared cold-start only. Caller supplies privileged current root; this
 * does not authenticate CPU state or authorize a new address space. Ready
 * describes the untouched initial service state, not general runtime health. */
bool rt863_bootstrap(uint64_t root);
bool rt863_bootstrap_ready(uint64_t root);
/* Ongoing quiescent service graph; trusted live root required. Does not
 * attest descriptors or enable user dispatch, even when true. */
bool rt864_services_boundary(uint64_t live_root);
bool rt858_validate(const struct rt858_expected *,const struct rt858_live *);
/* Exactly once, fail closed after a partial initialization. No task admission,
 * IRQ activation, allocator transfer, or device I/O. */
bool rt858_start(const struct rt858_expected *,const struct rt858_live *);
bool rt858_ready(uint64_t root);
#ifdef STYLEOS_TEST_RUNTIME_JOIN_861
/* Experimental safe-boundary accessor; no new dispatcher, no boot reset.
 * Caller supplies a trusted live CPU snapshot after restoring original state.
 * Not hardware proof, general scheduler admission, or a user ABI. */
struct dpr_service;
struct dpr_service *rt861_dispatcher(const struct rt858_live *);
#endif
#endif
