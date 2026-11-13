#ifndef STYLEOS_EXIT_BOOT_H
#define STYLEOS_EXIT_BOOT_H
#include "efi.h"
#include <styleos/boot_info.h>

#define STYLEOS_EBS_MAX_ATTEMPTS 8u
#define STYLEOS_MAX_MEMORY_MAP (UINT64_C(8) * 1024u * 1024u)

/* On success, the map allocation remains live and its physical address is
 * handed to the kernel. No boot-service call is legal after this returns OK. */
EFI_STATUS styleos_exit_boot_services(struct efi_boot_services *, EFI_HANDLE,
                                     struct boot_info *);
#endif
