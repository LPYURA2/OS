#include <styleos/stylefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
enum { NS = SFS_V2_MIN_SECTORS, SZ = NS * SFS_SECTOR_BYTES };
static uint8_t media[SZ], durable[SZ], snapshot[SZ], dirty[NS];
static unsigned operations, cut, reads, writes;
static struct sfs_volume fs;
static struct sfs_workspace workspace;
static uint8_t sample[257 * 1024 + 19], extract[sizeof(sample)], old[1490], replacement[1700];

static bool rd(void *c, uint32_t lba, uint8_t out[512])
{
    CHECK(c == media && lba < NS);
    ++reads;
    memcpy(out, media + (size_t)lba * 512, 512);
    return true;
}
static bool wr(void *c, uint32_t lba, const uint8_t in[512])
{
    CHECK(c == media && lba < NS);
    ++writes;
    if (++operations == cut) {
        memcpy(durable + (size_t)lba * 512, in, 13);
        return false;
    }
    memcpy(media + (size_t)lba * 512, in, 512);
    dirty[lba] = true;
    return true;
}
static bool flush(void *c)
{
    CHECK(c == media);
    if (++operations == cut) return false;
    for (unsigned i = 0; i < NS; ++i) if (dirty[i]) {
        memcpy(durable + (size_t)i * 512, media + (size_t)i * 512, 512);
        dirty[i] = false;
    }
    return true;
}
static const struct sfs_io io = {media, NS, rd, wr, flush};

static bool supply(void *ctx, uint32_t offset, uint8_t *data, size_t size)
{
    CHECK(offset <= sizeof(sample) && size <= sizeof(sample) - offset);
    memcpy(data, (const uint8_t *)ctx + offset, size);
    return true;
}
static bool receive(void *ctx, uint32_t offset, const uint8_t *data, size_t size)
{
    CHECK(offset <= sizeof(extract) && size <= sizeof(extract) - offset);
    memcpy((uint8_t *)ctx + offset, data, size);
    return true;
}
static void expect(unsigned id, const uint8_t *data, size_t size)
{
    CHECK(sfs_entry_at(&fs, id) && sfs_entry_at(&fs, id)->size == size);
    CHECK(sfs_read_to(&fs, id, receive, extract) == SFS_OK);
    CHECK(!memcmp(extract, data, size));
}

static void streaming_and_limits(void)
{
    CHECK(sfs_capacity_valid(NS) && sfs_capacity_valid(SFS_V2_MAX_SECTORS));
    CHECK(!sfs_capacity_valid(NS - 1) && !sfs_capacity_valid(SFS_V2_MAX_SECTORS + 2048));
    CHECK(sfs_mount(&fs, &workspace, &io) == SFS_SIGNATURE && writes == 0);
    CHECK(sfs_format(&fs, &workspace, &io) == SFS_OK && fs.format == 2);
    CHECK(sfs_name_limit(&fs) == 111 && sfs_file_limit(&fs) == 64 * 1024 * 1024);
    unsigned folder, origin, twin;
    const char name[] = "Документ с длинным именем для проверки StyleFS версии два.bin";
    CHECK(sizeof(name) > SFS_V1_NAME_MAX && sizeof(name) <= SFS_NAME_MAX + 1);
    CHECK(sfs_mkdir(&fs, &workspace, SFS_ROOT, "Проверка", &folder) == SFS_OK);
    CHECK(sfs_create_from(&fs, &workspace, folder, name, sizeof(sample), supply, sample, &origin) == SFS_OK);
    CHECK(sfs_copy(&fs, &workspace, origin, SFS_ROOT, "копия.bin", &twin) == SFS_OK);
    CHECK(sfs_move(&fs, &workspace, twin, folder, "копия.bin") == SFS_OK);
    unsigned previous_writes = writes;
    reads = 0;
    CHECK(sfs_mount(&fs, &workspace, &io) == SFS_OK);
    CHECK(writes == previous_writes && reads == 131); /* 1 super, 2 headers, 2x64 metadata sectors. */
    expect(origin, sample, sizeof(sample));
    expect(twin, sample, sizeof(sample));
    uint32_t a = fs.entries[origin].start, b = fs.entries[twin].start;
    CHECK(a != b && (a > b ? a - b : b - a) >= (sizeof(sample) + 511) / 512);
    media[(size_t)b * 512] ^= 1;
    CHECK(sfs_read_to(&fs, twin, NULL, NULL) == SFS_CORRUPT);
    media[(size_t)b * 512] ^= 1;
    CHECK(sfs_create(&fs, &workspace, folder, "oversize.bin", sample, (size_t)SFS_V2_FILE_MAX + 1, 0) == SFS_RANGE);
    CHECK(sfs_remove(&fs, &workspace, folder) == SFS_NOT_EMPTY);
    puts("PASS streaming >64 KiB, long UTF-8 names, no data scan at mount, contiguous allocation and content CRC");
}

static void interrupts(void)
{
    unsigned id;
    CHECK(sfs_create(&fs,&workspace,0,"replace.bin",old,sizeof(old),&id)==SFS_OK);
    uint32_t first = fs.entries[id].start;
    CHECK(sfs_write(&fs,&workspace,id,replacement,sizeof(replacement))==SFS_OK);
    uint32_t second = fs.entries[id].start;
    CHECK(second != first && fs.previous[id].start == first);
    memcpy(snapshot, durable, SZ);
    operations = 0;
    CHECK(sfs_write(&fs,&workspace,id,old,sizeof(old))==SFS_OK);
    uint32_t third = fs.entries[id].start;
    CHECK(third != second && third != first);
    CHECK(operations == 71); /* 3 payload sectors, 64 metadata, 1 header and 3 flushes. */
    for (unsigned i = 1; i <= 71; ++i) {
        memcpy(media, snapshot, SZ);
        memcpy(durable, snapshot, SZ);
        memset(dirty, 0, sizeof(dirty));
        cut = operations = 0;
        CHECK(sfs_mount(&fs, &workspace, &io) == SFS_OK);
        CHECK(fs.entries[id].start == second && fs.previous[id].start == first);
        cut = i;
        CHECK(sfs_write(&fs,&workspace,id,old,sizeof(old))==SFS_IO && !fs.mounted);
        CHECK(operations == i);
        memcpy(media, durable, SZ);
        cut = operations = 0;
        unsigned before = writes;
        CHECK(sfs_mount(&fs, &workspace, &io) == SFS_OK && before == writes);
        expect(id, replacement, sizeof(replacement));
    }
    memset(dirty, 0, sizeof(dirty));
    CHECK(sfs_write(&fs,&workspace,id,old,sizeof(old))==SFS_OK);
    CHECK(sfs_mount(&fs, &workspace, &io) == SFS_OK);
    expect(id, old, sizeof(old));
    unsigned newest = fs.active ? 66 : 1;
    media[(size_t)(newest + 1) * 512 + 9] ^= 1;
    CHECK(sfs_mount(&fs, &workspace, &io) == SFS_OK);
    expect(id, replacement, sizeof(replacement));
    memcpy(media, durable, SZ);
    CHECK(sfs_mount(&fs, &workspace, &io) == SFS_OK);
    puts("PASS 71 interrupted writes/flushes, torn header, oldest retained extent and metadata fallback");
}

static void fill_directory(void)
{
    unsigned directory;
    CHECK(sfs_mkdir(&fs,&workspace,0,"full",&directory)==SFS_OK);
    unsigned added = 0;
    enum sfs_result r;
    do {
        char name[32];
        snprintf(name,sizeof(name),"file-%u",added);
        r = sfs_create(&fs,&workspace,directory,name,NULL,0,NULL);
        if (r == SFS_OK) ++added;
    } while (r == SFS_OK && added < SFS_ENTRIES);
    CHECK(r == SFS_FULL && added > SFS_V1_ENTRIES);
    CHECK(sfs_mount(&fs,&workspace,&io)==SFS_OK);
    CHECK(sfs_find(&fs,directory,"file-100")>=0);
    puts("PASS hundreds of directory entries persist; limit returns FULL");
}

int main(void)
{
    for (unsigned i=0;i<sizeof(sample);++i) sample[i]=(uint8_t)(i*17+3);
    memset(old,0x21,sizeof(old));
    memset(replacement,0x93,sizeof(replacement));
    streaming_and_limits();
    interrupts();
    fill_directory();
    puts("STYLEFS V2 HOST: PASS");
    return 0;
}
