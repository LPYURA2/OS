#ifndef STYLEOS_RUNTIME_BANK_862_H
#define STYLEOS_RUNTIME_BANK_862_H
#include <styleos/runtime_select_862.h>
#include <styleos/runtime_tick_860.h>
/* Experimental, serialized supervisor-only bank. No lifecycle state, current
 * slot or scheduling cursor: the existing registry/dispatcher own these.
 * The architecture must authenticate the live CPU/IRQ/FP and audit mappings.
 * Inputs are trusted, distinct kernel objects; no user pointer ABI. */
struct rt862_bank_slot { uint32_t pid; uint64_t handle, root; struct up_context context; };
struct rt862_bank { uint64_t safe; uint16_t cs,ss; struct rt862_bank_slot slots[PS_MAX_SLOTS]; };
bool rt862_bank_init(struct rt862_bank *,struct dpr_service *,uint64_t,uint16_t,uint16_t);
bool rt862_bank_enter(const struct rt862_bank *,const struct dpr_service *,const struct rt862_selection *,uint64_t,struct up_context *);
/* Only after a real safe-root return, FP suspension and EOI. */
enum ps_status rt862_bank_tick(struct rt862_bank *,struct dpr_service *,const struct rt862_selection *,uint64_t,const struct rt860_tick *);
/* Narrow terminal adapter: exit INT80 or ordinary user #PF. No general ABI. */
enum ps_status rt862_bank_finish(struct rt862_bank *,struct dpr_service *,const struct rt862_selection *,uint64_t,uint64_t,const struct s847_raw *,uint64_t,uint64_t);
/* Serialized safe-root admission into a vacant slot. Architecture must have
 * already established an independently audited mapping and FP context. */
/* Empty bank may reopen after all slots are retired; never reset PID or dispatch counters. */
bool rt862_bank_open(struct rt862_bank *,struct dpr_service *,uint64_t,uint16_t,uint16_t);
enum ps_status rt862_bank_admit(struct rt862_bank *,struct dpr_service *,uint64_t,unsigned,uint64_t,uint64_t,const char *,uint32_t *);
#ifdef STYLEOS_OWNED_SHELL_865
/* Trusted supervisor only; architecture proves safe CR3, IF=0, live lease
 * and quiescence. No fabricated dispatch or trap. READY or WAITING. */
enum ps_status rt865_bank_stop_ready(struct rt862_bank *,struct dpr_service *,
 uint64_t,unsigned,uint32_t,uint64_t,uint64_t,bool);
/* Kernel-only saved context transition, exact slot/PID/generation/root.
 * Does not prove or consume a wake event. No user ABI. */
enum ps_status rt865_bank_wait_state(struct rt862_bank *,struct dpr_service *,
 uint64_t,unsigned,uint32_t,uint64_t,uint64_t,bool);
#endif
/* Architecture must first revoke/scrub mappings, lease and FP state. */
enum ps_status rt862_bank_release(struct rt862_bank *,struct dpr_service *,uint64_t,unsigned,uint32_t,uint64_t,uint64_t);
#if (defined(STYLEOS_TEST_RUNTIME_SYSCALL_863) || defined(STYLEOS_OWNED_SHELL_865))
/* Admission from a trusted loader's already audited initial context.
 * Fixed S846 address bounds; no user pointer, loader or mapping authority.
 * The scheduler is quiescent. Validate before publishing PID or context.
 * Failure preserves registry, bank and output. */
enum ps_status rt863_bank_admit_initial(struct rt862_bank *,struct dpr_service *,
 uint64_t,unsigned,uint64_t,uint64_t,const char *,const struct up_context *,uint32_t *);
#if defined(STYLEOS_INTEGRATED_RUNTIME_864) || defined(STYLEOS_TEST_RUNTIME_IMAGE_ADMISSION_865)
#include <styleos/native_image_slot_865.h>
/* Trusted loader admission for the existing bounded three-page layout.
 * The architecture MUST independently audit the actual tables, current
 * arena lease and code provenance immediately before this call. Metadata
 * alone is not a hardware proof or authorization of a .tea package.
 * No write or process publication on refusal. */
enum ps_status rt865_bank_admit_image(struct rt862_bank *,struct dpr_service *,
 uint64_t,unsigned,const struct s846_root *,const struct s844_lease *,
 const struct nte_loaded *,const char *,const struct up_context *,uint32_t *);
/* Kernel-owned request. The default zero GPR array is a native entry context;
 * no workload identity or expected instruction sites are required.
 * The reader and audit are trusted, serialized, non-mutating capabilities.
 * Caller owns the detached root, live lease, FP admission and all table
 * mappings; it must retire them on any refusal. A post-load refusal can
 * leave validated bytes in the unpublished lease, never a published PID.
 * Scratch is cleared after the read; outputs are committed only on success. */
struct rt865_launch_request {
 struct s865_image_source source;
 char name[PR_LABEL_BYTES];
 uint64_t gpr[15];
};
enum ps_status rt865_launch_prepared(struct rt862_bank *,struct dpr_service *,
 uint64_t,unsigned,const struct s846_root *,const struct s844_arena *,
 const struct s844_lease *,const struct rt865_launch_request *,
 uint8_t *,size_t,bool (*)(void *),void *,struct nte_loaded *,uint32_t *);
#endif
/* Supervisor-only, after authenticated safe-root return and FP suspension.
 * Nonterminal INT80 range/yield/unknown; no user-memory copy or I/O. */
enum ps_status rt863_bank_syscall(struct rt862_bank *,struct dpr_service *,
 const struct rt862_selection *,uint64_t,uint64_t,const struct s847_raw *,uint64_t,uint64_t);
/* Common timer/syscall/terminal receipt; hardware proof belongs to caller. */
enum ps_status rt863_bank_event(struct rt862_bank *,struct dpr_service *,const struct rt862_selection *,uint64_t,const struct rt860_tick *,uint64_t);
#endif
#ifdef STYLEOS_TEST_RUNTIME_COOP_863
bool rt863_coop_enter(const struct rt862_bank *,const struct dpr_service *,
 const struct rt862_selection *,uint64_t,struct up_context *);
enum ps_status rt863_coop_commit(struct rt862_bank *,struct dpr_service *,
 const struct rt862_selection *,uint64_t,uint64_t,const struct s847_raw *,uint64_t,uint64_t);
#endif
#endif
