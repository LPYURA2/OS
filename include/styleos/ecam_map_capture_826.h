#ifndef STYLEOS_ECAM_MAP_CAPTURE_826_H
#define STYLEOS_ECAM_MAP_CAPTURE_826_H
#include <styleos/boot_info.h>
/* Diagnostic only, after successful EBS from the trusted local handoff.
 * The allocation lifetime is guaranteed by exit_boot.c. Never accepts arbitrary
 * caller-controlled addresses, never writes the map or grants ECAM/DMA access. */
#define STYLEOS826_MAP_LIMIT 32768u
int styleos826_emit_final_map(const struct boot_info *boot);
#endif
