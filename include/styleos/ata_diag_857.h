#ifndef STYLEOS_ATA_DIAG_857_H
#define STYLEOS_ATA_DIAG_857_H
#include <stdbool.h>
#include <stdint.h>
/* Dedicated disposable Q35 fixture, not a general ATA controller grant.
 * Caller must check same-boot host fixture and live context for every callback. */
#define ATA857_SECTORS 4096u
#define ATA857_WORDS 256u
#define ATA857_POLLS 4096u
struct ata857_ops {
 bool (*status)(void *,uint8_t *); /* alternate status only */
 bool (*write_reg)(void *,unsigned,uint8_t); /* task file, never data port */
 bool (*read_data)(void *,uint16_t *);
};
#define ATA_LBA28_SECTORS UINT32_C(0x10000000)
struct ata857_state {bool identified,busy,quarantined;uint32_t sectors;};
/* Explicit capacity profile for a separately attested secondary-master
 * device. Does not change the default 4096-sector policy or authorize ports.
 * The platform must bind the exact expected geometry and separately allow
 * the larger task-file register values only for the selected profile. */
bool ata857_identify_capacity(struct ata857_state *,const struct ata857_ops *,void *,
                              uint32_t expected,uint16_t out[ATA857_WORDS]);
bool ata857_geometry_capacity(const uint16_t words[ATA857_WORDS],uint32_t expected);
bool ata857_allowed_lba28(unsigned reg,uint8_t value);
bool ata857_identify(struct ata857_state *,const struct ata857_ops *,void *,uint16_t out[ATA857_WORDS]);
bool ata857_read(struct ata857_state *,const struct ata857_ops *,void *,uint32_t lba,uint16_t out[ATA857_WORDS]);
/* Optional bounded pause between BUSY samples after the one READ command.
 * Same poll ceiling; no retries, new command or changed status checks. */
bool ata857_read_paced(struct ata857_state *,const struct ata857_ops *,void *,uint32_t,uint16_t out[ATA857_WORDS],bool (*pace)(void *));
bool ata857_geometry(const uint16_t words[ATA857_WORDS]);
bool ata857_allowed(unsigned reg,uint8_t value);
#endif
