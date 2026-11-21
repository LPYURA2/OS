#ifndef STYLEOS_DESKTOP_VOLUME_IO_H
#define STYLEOS_DESKTOP_VOLUME_IO_H

#include <styleos/stylefs.h>

/* Explicit, bounded partition/whole-drive mapping for one independently
 * mounted StyleFS volume. No disk probing, formatting or implicit writes.
 * A map and the physical device callbacks must remain valid while mounted.
 */
struct desk_physical_disk {
    void *context;
    uint32_t sectors;
    bool (*read)(void *, uint32_t, uint8_t[SFS_SECTOR_BYTES]);
    bool (*write)(void *, uint32_t, const uint8_t[SFS_SECTOR_BYTES]);
    bool (*flush)(void *);
};

struct desk_volume_map {
    struct desk_physical_disk disk;
    uint32_t start_sector;
    bool writable;
    struct sfs_io io;
};

/* Reject invalid bounds, unsupported StyleFS size and incomplete callbacks.
 * Never attempt auto-format: a blank or unknown disk remains untouched.
 */
bool desk_volume_map_init(struct desk_volume_map *map,
                          const struct desk_physical_disk *disk,
                          uint32_t start_sector, uint32_t length_sectors,
                          bool writable);

#endif
