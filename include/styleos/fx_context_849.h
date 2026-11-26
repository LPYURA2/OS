#ifndef STYLEOS_FX_CONTEXT_849_H
#define STYLEOS_FX_CONTEXT_849_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define S849_BYTES 512u
#define S849_MXCSR_FALLBACK UINT32_C(0xffbf)
/* Bounded legacy x87/MMX/SSE context, not XSAVE or AVX. Only trusted kernel
 * storage may be supplied to FXRSTOR, after mode and image validation. */
struct s849_image { _Alignas(16) uint8_t bytes[S849_BYTES]; };
struct s849_caps { uint32_t leaf1_ecx,leaf1_edx,mxcsr_mask; uint64_t cr0,cr4; };
struct s849_slot { struct s849_image image; uint64_t root,generation; bool live; };
#ifdef STYLEOS_TEST_WIDE_FX_864
#define S849_SLOT_COUNT 4u
#else
#define S849_SLOT_COUNT 2u
#endif
struct s849_bank { struct s849_slot slot[S849_SLOT_COUNT]; uint32_t mxcsr_mask; int active; bool admitted; uint64_t last_generation; };
_Static_assert(sizeof(struct s849_image)==512,"FXSAVE extent");
_Static_assert(_Alignof(struct s849_image)==16,"FXSAVE alignment");
bool s849_admit(const struct s849_caps *,uint32_t *);
bool s849_image_valid(const struct s849_image *,uint32_t);
/* Compare defined architectural state, not reserved bytes or inactive x87 payloads. */
bool s849_arch_equal(const struct s849_image *,const struct s849_image *,uint32_t);
#ifdef STYLEOS_TEST_WIDE_FX_864
/* Open an empty bank and admit one audited root at a time. Generations
 * must strictly increase across slot reuse within this bank lifetime. */
bool s865_fx_open(struct s849_bank *,const struct s849_caps *);
bool s865_fx_bind(struct s849_bank *,unsigned,uint64_t,uint64_t);
/* Trusted caller binds each entry to a distinct, already audited root and
 * its current physical lease generation. No actual CPU mode transition. */
bool s849_init_set(struct s849_bank *,const struct s849_caps *,
 const uint64_t roots[S849_SLOT_COUNT],const uint64_t generations[S849_SLOT_COUNT]);
#else
bool s849_init(struct s849_bank *,const struct s849_caps *,uint64_t,uint64_t);
#endif
/* Dispatch/suspend metadata only. Caller must serialize and perform actual
 * restore/save before changing execution domain, without using FP in kernel. */
bool s849_dispatch(struct s849_bank *,unsigned,uint64_t,uint64_t,struct s849_image *);
bool s849_suspend(struct s849_bank *,unsigned,uint64_t,uint64_t,const struct s849_image *);
bool s849_retire(struct s849_bank *,unsigned,uint64_t,uint64_t);
#endif
