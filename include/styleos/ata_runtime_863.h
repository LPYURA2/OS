#ifndef STYLEOS_ATA_RUNTIME_863_H
#define STYLEOS_ATA_RUNTIME_863_H
#include <styleos/ata_diag_857.h>
/* A single fixed secondary-master PIO fixture. Backend must attest the
 * device/ports and DMA denial before EVERY I/O callback. No raw I/O here. */
struct ata_runtime_ops_863 {
    struct ata857_ops read_ops;
    bool (*write_data)(void *,uint16_t);
};
struct ata_runtime_863 {
    struct ata857_state reader;
    const struct ata_runtime_ops_863 *ops;
    void *ctx;
    bool bound,busy,quarantined,flush_supported,pending_flush;
};
bool ata_runtime_bind_863(struct ata_runtime_863 *,const struct ata_runtime_ops_863 *,void *);
bool ata_runtime_read_863(void *,uint32_t,uint8_t [512]);
bool ata_runtime_write_863(void *,uint32_t,const uint8_t [512]);
bool ata_runtime_flush_863(void *);
#endif
