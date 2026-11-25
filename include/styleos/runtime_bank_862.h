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
bool rt862_bank_open(struct rt862_bank *,struct dpr_service *,uint64_t,uint16_t,uint16_t);
enum ps_status rt862_bank_admit(struct rt862_bank *,struct dpr_service *,uint64_t,unsigned,uint64_t,uint64_t,const char *,uint32_t *);
/* Architecture must first revoke/scrub mappings, lease and FP state. */
enum ps_status rt862_bank_release(struct rt862_bank *,struct dpr_service *,uint64_t,unsigned,uint32_t,uint64_t,uint64_t);
#endif
