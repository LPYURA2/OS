#include <styleos/desktop_extra_volume.h>
#include <styleos/desktop_volume_io.h>
#include <styleos/desktop_v0.h>
#include <styleos/arch.h>

/* Independent Data-2 volume; only backing service, no GUI redesign. */
static struct {
    struct desk_volume_map mapping;
    struct sfs_volume volume;
    struct sfs_workspace workspace;
} second;

static bool read2(void *ctx, uint32_t lba, uint8_t data[512])
{ (void)ctx; return desk_ata_read_data2(lba, data); }
static bool write2(void *ctx, uint32_t lba, const uint8_t data[512])
{ (void)ctx; return desk_ata_write_data2(lba, data); }
static bool flush2(void *ctx)
{ (void)ctx; return desk_ata_flush_data2(); }

const struct sfs_volume *desk_extra_volume_readonly(void)
{ return second.volume.mounted ? &second.volume : 0; }

void desk_extra_volume_mount(void)
{
    const uint32_t sectors = desk_ata_probe_data2();
    if (!sfs_capacity_valid(sectors)) {
        arch_puts("STYLEOS DESKTOP DATA2 ABSENT OR UNSUPPORTED\n");
        return;
    }
    const struct desk_physical_disk disk = { 0, sectors, read2, write2, flush2 };
    if (!desk_volume_map_init(&second.mapping, &disk, 0, sectors, true)) {
        arch_puts("STYLEOS DESKTOP DATA2 MAPPING REFUSED\n");
        return;
    }
    if (sfs_mount(&second.volume, &second.workspace, &second.mapping.io) != SFS_OK) {
        arch_puts("STYLEOS DESKTOP DATA2 UNRECOGNIZED - UNMODIFIED\n");
        return;
    }
    arch_puts(second.volume.format == 2 ?
        "STYLEOS DESKTOP DATA2 STYLEFS V2 MOUNT\n" :
        "STYLEOS DESKTOP DATA2 STYLEFS V1 MOUNT\n");
    for (unsigned i = 1; i < SFS_ENTRIES; ++i) {
        const struct sfs_entry *e = sfs_entry_at(&second.volume, i);
        if (e && e->parent == SFS_ROOT && e->type == SFS_FILE) {
            arch_puts("STYLEOS DESKTOP DATA2 ROOT FILE FOUND\n");
            break;
        }
    }
}
