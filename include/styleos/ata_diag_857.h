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
struct ata857_state {bool identified,busy,quarantined;};
bool ata857_identify(struct ata857_state *,const struct ata857_ops *,void *,uint16_t out[ATA857_WORDS]);
bool ata857_read(struct ata857_state *,const struct ata857_ops *,void *,uint32_t lba,uint16_t out[ATA857_WORDS]);
bool ata857_geometry(const uint16_t words[ATA857_WORDS]);
bool ata857_allowed(unsigned reg,uint8_t value);
#endif
