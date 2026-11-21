#include <styleos/desktop_volume_io.h>

static bool mapped_read(void *context, uint32_t lba, uint8_t data[SFS_SECTOR_BYTES])
{
    const struct desk_volume_map *map = context;
    if (!map || lba >= map->io.sectors ||
        lba >= map->disk.sectors - map->start_sector) return false;
    return map->disk.read(map->disk.context, map->start_sector + lba, data);
}

static bool mapped_write(void *context, uint32_t lba,
                         const uint8_t data[SFS_SECTOR_BYTES])
{
    const struct desk_volume_map *map = context;
    if (!map || !map->writable || !map->disk.write ||
        lba >= map->io.sectors ||
        lba >= map->disk.sectors - map->start_sector) return false;
    return map->disk.write(map->disk.context, map->start_sector + lba, data);
}

static bool mapped_flush(void *context)
{
    const struct desk_volume_map *map = context;
    return map && map->writable && map->disk.flush &&
           map->disk.flush(map->disk.context);
}

bool desk_volume_map_init(struct desk_volume_map *map,
                          const struct desk_physical_disk *disk,
                          uint32_t start_sector, uint32_t length_sectors,
                          bool writable)
{
    if (!map || !disk || !disk->read ||
        (writable && (!disk->write || !disk->flush)) ||
        start_sector > disk->sectors ||
        length_sectors > disk->sectors - start_sector ||
        !sfs_capacity_valid(length_sectors)) return false;
    map->disk = *disk;
    map->start_sector = start_sector;
    map->writable = writable;
    map->io = (struct sfs_io){
        .context = map,
        .sectors = length_sectors,
        .read = mapped_read,
        .write = writable ? mapped_write : 0,
        .flush = writable ? mapped_flush : 0
    };
    return true;
}
