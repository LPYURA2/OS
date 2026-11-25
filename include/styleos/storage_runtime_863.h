#ifndef STYLEOS_STORAGE_RUNTIME_863_H
#define STYLEOS_STORAGE_RUNTIME_863_H
#include <stdbool.h>
#include <stdint.h>
#define S863_SECTOR_BYTES 512u
#define S863_DISK_SECTORS 4096u
/* Bounded disposable data-disk candidate, not a general device grant.
 * The platform must independently attest the backend and observations.
 * No direct hardware instructions, DMA mapping or filesystem policy here. */
struct storage_obs_863 {
    uint64_t root, epoch, device;
    uint32_t sectors;
    bool post_ebs, single_cpu, kernel_context, exclusive_pio, dma_denied;
    bool disposable_data, flush_supported;
};
struct storage_ops_863 {
    bool (*observe)(void *,struct storage_obs_863 *);
    bool (*read)(void *,uint32_t,uint8_t [S863_SECTOR_BYTES]);
    bool (*write)(void *,uint32_t,const uint8_t [S863_SECTOR_BYTES]);
    bool (*flush)(void *);
};
struct storage_runtime_863 {
    const struct storage_ops_863 *ops;
    void *ctx;
    uint64_t root,epoch,device;
    uint32_t first,count;
    bool writable,bound,busy,quarantined;
};
bool storage_bind_863(struct storage_runtime_863 *,const struct storage_ops_863 *,
    void *,uint64_t,uint64_t,uint64_t,uint32_t,uint32_t,bool);
bool storage_read_863(struct storage_runtime_863 *,uint32_t,uint8_t [S863_SECTOR_BYTES]);
/* Single-sector verified commit: write, flush, then readback. A failure may
 * leave changed media; quarantine prevents reuse, not atomic rollback. */
bool storage_commit_863(struct storage_runtime_863 *,uint32_t,const uint8_t [S863_SECTOR_BYTES]);
#endif
