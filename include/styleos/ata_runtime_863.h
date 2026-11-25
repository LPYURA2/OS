#ifndef STYLEOS_ATA_RUNTIME_863_H
#define STYLEOS_ATA_RUNTIME_863_H
#include <styleos/ata_diag_857.h>
/* A single fixed secondary-master PIO fixture. Backend must attest the
 * device/ports and DMA denial before EVERY I/O callback. No raw I/O here. */
struct ata_runtime_ops_863 {
    struct ata857_ops read_ops;
    bool (*write_data)(void *,uint16_t);
};
/* Completion of the already issued WRITE/FLUSH may wait for host I/O.
 * Only those two completion waits receive this larger finite poll budget.
 * The platform's existing cumulative guard remains authoritative.
 * This is a fixture-specific poll ceiling, not a wall-clock ATA timeout. */
#define ATA863_COMPLETION_POLLS (8u * ATA857_POLLS)
/* First fault is sticky until a fresh unbound context. No retry or reset. */
enum ata863_fault {
    ATA863_NONE, ATA863_IDENTIFY, ATA863_FLUSH_UNSUPPORTED,
    ATA863_STATUS_IO, ATA863_STATUS_INVALID, ATA863_STATUS_ERROR,
    ATA863_STATUS_TIMEOUT, ATA863_REGISTER_IO, ATA863_DATA_IO,
    ATA863_READ_TRANSFER
};
enum ata863_phase {
    ATA863_IDLE, ATA863_BIND, ATA863_READ, ATA863_WRITE_SETUP,
    ATA863_WRITE_DATA, ATA863_WRITE_COMPLETE, ATA863_FLUSH
};
struct ata_runtime_863 {
    struct ata857_state reader;
    const struct ata_runtime_ops_863 *ops;
    void *ctx;
    bool bound,busy,quarantined,flush_supported,pending_flush;
    enum ata863_fault fault;
    enum ata863_phase phase;
    unsigned status_polls;
    uint8_t last_status;
};
bool ata_runtime_bind_863(struct ata_runtime_863 *,const struct ata_runtime_ops_863 *,void *);
bool ata_runtime_read_863(void *,uint32_t,uint8_t [512]);
bool ata_runtime_write_863(void *,uint32_t,const uint8_t [512]);
bool ata_runtime_flush_863(void *);
#endif
