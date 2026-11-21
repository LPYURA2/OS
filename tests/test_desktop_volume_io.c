/* Host exercises the production disk-offset mapping and actual StyleFS v2.
 * Two independently mounted volumes, unknown media, read-only, bounds.
 */
#include <styleos/desktop_volume_io.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OFFSET = 2048, CAPACITY = SFS_V2_MIN_SECTORS, PHYSICAL = CAPACITY + 2 * OFFSET };
struct memdisk {
    uint8_t *data;
    unsigned reads, writes, flushes;
};
static struct desk_volume_map maps[2], readonly_map;
static struct sfs_volume vols[2], read_only;
static struct sfs_workspace spaces[2], read_only_space;

static bool rd(void *ctx, uint32_t lba, uint8_t out[512])
{
    struct memdisk *d = ctx;
    if (lba >= PHYSICAL) return false;
    ++d->reads;
    memcpy(out, d->data + (size_t)lba * 512, 512);
    return true;
}
static bool wr(void *ctx, uint32_t lba, const uint8_t in[512])
{
    struct memdisk *d = ctx;
    if (lba >= PHYSICAL) return false;
    ++d->writes;
    memcpy(d->data + (size_t)lba * 512, in, 512);
    return true;
}
static bool sync_disk(void *ctx)
{
    ++((struct memdisk *)ctx)->flushes;
    return true;
}

int main(void)
{
    struct memdisk disks[2] = {{0}, {0}};
    for (int i = 0; i < 2; ++i) {
        disks[i].data = calloc(PHYSICAL, 512);
        assert(disks[i].data);
    }
    const struct desk_physical_disk devices[2] = {
        {&disks[0], PHYSICAL, rd, wr, sync_disk},
        {&disks[1], PHYSICAL, rd, wr, sync_disk}
    };
    assert(!desk_volume_map_init(NULL, &devices[0], OFFSET, CAPACITY, true));
    assert(!desk_volume_map_init(&maps[0], NULL, OFFSET, CAPACITY, true));
    assert(!desk_volume_map_init(&maps[0], &devices[0], UINT32_MAX, CAPACITY, true));
    assert(!desk_volume_map_init(&maps[0], &devices[0], PHYSICAL - CAPACITY + 1, CAPACITY, true));
    assert(!desk_volume_map_init(&maps[0], &devices[0], OFFSET, CAPACITY - 1, true));
    struct desk_physical_disk damaged = devices[0];
    damaged.write = NULL;
    assert(!desk_volume_map_init(&maps[0], &damaged, OFFSET, CAPACITY, true));
    damaged = devices[0]; damaged.flush = NULL;
    assert(!desk_volume_map_init(&maps[0], &damaged, OFFSET, CAPACITY, true));

    for (int i = 0; i < 2; ++i) {
        memset(disks[i].data, 0xa5, 512);
        memset(disks[i].data + (size_t)(OFFSET + CAPACITY) * 512, 0x5a, 512);
        assert(desk_volume_map_init(&maps[i], &devices[i], OFFSET, CAPACITY, true));
        assert(maps[i].io.sectors == CAPACITY);
        assert(sfs_mount(&vols[i], &spaces[i], &maps[i].io) == SFS_SIGNATURE);
        assert(disks[i].writes == 0);
        assert(sfs_format(&vols[i], &spaces[i], &maps[i].io) == SFS_OK);
        assert(sfs_mount(&vols[i], &spaces[i], &maps[i].io) == SFS_OK);
        assert(vols[i].format == 2);
    }

    unsigned a = 0, b = 0;
    const uint8_t text0[] = "volume A only", text1[] = "other data";
    assert(sfs_create(&vols[0], &spaces[0], SFS_ROOT, "a.txt", text0, sizeof(text0), &a) == SFS_OK);
    assert(sfs_create(&vols[1], &spaces[1], SFS_ROOT, "b.txt", text1, sizeof(text1), &b) == SFS_OK);
    assert(sfs_find(&vols[0], SFS_ROOT, "b.txt") == -1);
    assert(sfs_find(&vols[1], SFS_ROOT, "a.txt") == -1);
    uint8_t readbuf[512] = {0};
    assert(sfs_read(&vols[0], a, readbuf, sizeof(readbuf)) == SFS_OK);
    assert(memcmp(readbuf, text0, sizeof(text0)) == 0);
    assert(sfs_read(&vols[1], b, readbuf, sizeof(readbuf)) == SFS_OK);
    assert(memcmp(readbuf, text1, sizeof(text1)) == 0);

    assert(desk_volume_map_init(&readonly_map, &devices[1], OFFSET, CAPACITY, false));
    assert(readonly_map.io.write == NULL && readonly_map.io.flush == NULL);
    assert(sfs_mount(&read_only, &read_only_space, &readonly_map.io) == SFS_OK);
    unsigned ignored = 0;
    unsigned old_writes = disks[1].writes;
    assert(sfs_create(&read_only, &read_only_space, SFS_ROOT, "reject.txt", text1,
                      sizeof(text1), &ignored) == SFS_STATE);
    assert(disks[1].writes == old_writes);
    assert(sfs_find(&read_only, SFS_ROOT, "b.txt") == (int)b);

    unsigned before = disks[0].reads;
    assert(!maps[0].io.read(maps[0].io.context, CAPACITY, readbuf));
    assert(disks[0].reads == before);
    before = disks[0].writes;
    assert(!maps[0].io.write(maps[0].io.context, CAPACITY, readbuf));
    assert(disks[0].writes == before);

    for (int i = 0; i < 2; ++i) {
        for (unsigned j = 0; j < 512; ++j) {
            assert(disks[i].data[j] == 0xa5);
            assert(disks[i].data[((size_t)OFFSET + CAPACITY) * 512 + j] == 0x5a);
        }
        assert(disks[i].writes && disks[i].flushes);
        free(disks[i].data);
    }
    puts("DESKTOP MULTIVOLUME HOST PASS: two real StyleFS v2 volumes, offset guards, read-only, no auto-format, bounded IO");
    return 0;
}
