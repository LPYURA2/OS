#ifndef STYLEOS_X86_DESKTOP_DISPLAY_H
#define STYLEOS_X86_DESKTOP_DISPLAY_H
#include "efi.h"

/* Read and copy mode metadata before ExitBootServices. No protocol is retained. */
void desktop_capture_uefi_mode(struct efi_boot_services *);

#endif
