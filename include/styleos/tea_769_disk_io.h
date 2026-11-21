#ifndef STYLEOS_TEA_769_DISK_IO_H
#define STYLEOS_TEA_769_DISK_IO_H
#include <stdint.h>
/* Only available in the dedicated x86 QEMU experiment. Never writes to disk.
 * Primary IDE master, LBA28, polled, no interrupts or DMA. */
int tea_769_disk_init(uint32_t *sectors);
int tea_769_disk_sector(uint32_t lba, uint8_t out[512]);
#endif
