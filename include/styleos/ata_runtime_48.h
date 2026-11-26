#ifndef STYLEOS_ATA_RUNTIME_48_H
#define STYLEOS_ATA_RUNTIME_48_H
#include <styleos/ata_runtime_863.h>
#define ATA48_SECTORS (UINT64_C(1)<<48)
/* Separate trusted secondary-master fixture transport. No raw ports, no
 * DMA, no reset, no discovery. Platform must attest exact device/context
 * and enforce register sequence/command policy before each callback.
 * This module is not linked into the existing guest profiles. */
struct ata_runtime_48 {
    const struct ata_runtime_ops_863 *ops;
    void *context;
    uint64_t sectors;
    bool busy,bound,quarantined,pending_flush;
};
bool ata48_geometry(const uint16_t words[256],uint64_t expected);
bool ata48_bind(struct ata_runtime_48 *,const struct ata_runtime_ops_863 *,
                void *,const char serial[20],uint64_t expected);
bool ata48_read(struct ata_runtime_48 *,uint64_t lba,uint8_t out[512]);
bool ata48_write(struct ata_runtime_48 *,uint64_t lba,const uint8_t in[512]);
bool ata48_flush(struct ata_runtime_48 *);
#endif
