/* 823: bounded, read-only copies of UEFI ACPI tables; TEST PROFILE ONLY. */
#ifndef STYLEOS_ACPI_FIRMWARE_823_H
#define STYLEOS_ACPI_FIRMWARE_823_H
#include <stdint.h>
#define STYLEOS823_MAX_SSDT 8u
struct styleos823_firmware {
 const uint8_t *dsdt;
 uint32_t dsdt_len;
 const uint8_t *ssdt[STYLEOS823_MAX_SSDT];
 uint32_t ssdt_len[STYLEOS823_MAX_SSDT];
 uint32_t ssdt_count;
 uint32_t mcfg_len;
 uint32_t root_count;
};
/* Returns 0 on ANY missing or invalid firmware snapshot; never maps MMIO. */
int styleos823_get_firmware(struct styleos823_firmware *out);
#endif
