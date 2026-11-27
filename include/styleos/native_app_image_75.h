#ifndef STYLEOS_NATIVE_APP_IMAGE_75_H
#define STYLEOS_NATIVE_APP_IMAGE_75_H
#include <styleos/native_tea_elf.h>
#include <styleos/physical_arena_844.h>
/* Detached preparation only. These are bounded initial-policy limits, not
 * a hardware address-space grant. Existing one-page ABI is unchanged. */
#define NAI75_CODE_PAGES 8u
#define NAI75_DATA_PAGES 16u
#define NAI75_STACK_PAGES 4u
#define NAI75_MAX_ELF (128u*1024u)
#define NAI75_DATA_VA UINT64_C(0x12000000)
#define NAI75_STACK_VA UINT64_C(0x11000000)
struct nai75_layout {
 uint64_t entry;
 uint32_t code_pages,data_pages,stack_pages,total_pages;
 uint32_t code_offset,code_file,code_mem,data_offset,data_file,data_mem;
};
/* Exactly two ordered PT_LOADs: RX code at fixed code VA, RW data at fixed
 * data VA. No dynamic/interpreter/relocation, no RWX. Stack is zeroed private
 * storage, never sourced from the ELF. Caller owns immutable input. */
enum nte_status nai75_inspect(const uint8_t *,size_t,const uint8_t[32],
                             unsigned stack_pages,struct nai75_layout *);
/* Trusted safe-root caller must own an unpublished detached lease and
 * serialize against dispatch. Check exact live lease and recompute layout,
 * verify hash before writes. On failure leave lease bytes and result intact.
 * Physical order: code, data, stack. This does not map or execute pages. */
enum nte_status nai75_stage(const struct s844_arena *,struct s844_lease,
 const uint8_t *,size_t,const uint8_t[32],unsigned,struct nai75_layout *);
#endif
