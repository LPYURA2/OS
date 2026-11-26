#ifndef STYLEOS_X64_NATIVE_EXECUTOR_865_H
#define STYLEOS_X64_NATIVE_EXECUTOR_865_H
#include <styleos/runtime_bank_862.h>
#include <styleos/runtime_entry_858.h>
/* Internal privileged single-CPU adapter. Caller owns descriptor setup,
 * admitted LAPIC, mapping audit and serialized safe-root state. */
struct desc848 {uint16_t limit;uint64_t base;}__attribute__((packed));
struct gate848 {uint16_t lo,cs;uint8_t ist,type;uint16_t mid;uint32_t hi,zero;}__attribute__((packed));
extern struct gate848 idt848[256];
extern struct dpr_service *runtime860;
extern struct rt862_bank bank862;
extern struct s847_raw pending860;
extern uint64_t cr2862,frame860,lapic848;
extern bool receipt860,active848,armed848;
struct rt865_slice {enum rt865_frame_kind kind;enum pr_state state;uint64_t ticks_before;};
struct rt865_slice run865(const struct rt862_selection *,const struct up_context *,
 const struct desc848 *,const struct desc848 *,const struct desc848 *,const struct desc848 *,
 const struct rt858_live *,bool (*)(void *),void *);
void s865_retire_terminal(const struct rt862_selection *,bool (*)(void *),
 bool (*)(void *,unsigned,uint64_t,uint64_t),void *);
#ifdef STYLEOS_NATIVE_EXECUTOR_OBSERVERS
bool s865_test_force_timer(unsigned,uint64_t,uint64_t);
void s865_test_observe_trap(unsigned,enum rt865_frame_kind,const struct s847_raw *,uint64_t,uint32_t);
#endif
#endif
