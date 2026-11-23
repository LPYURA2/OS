/* StyleOS 821: explicit minimal ACPICA EFI platform integer prelude.
 * Used only by pinned freestanding ACPICA x86-64 COFF diagnostics.
 * acefi.h declares efi_call* with UINT64 before actypes.h defines it.
 * No firmware access or DMA permission is enabled by this shim.
 */
#ifndef STYLEOS_ACPICA_EFI_PRELUDE_821_H
#define STYLEOS_ACPICA_EFI_PRELUDE_821_H
#include <stdint.h>
typedef uint64_t UINT64;
#endif
