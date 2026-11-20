#include <styleos/stylefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
enum { DISK_BYTES = SFS_DISK_SECTORS * SFS_SECTOR_BYTES };
static uint8_t medium[DISK_BYTES], durable[DISK_BYTES], baseline[DISK_BYTES];
static uint32_t cutoff, ops, writes, failing_read = UINT32_MAX;
static bool tear;
static struct sfs_volume v;
static struct sfs_workspace w;
static uint8_t old_data[1400], new_data[1700], output[SFS_FILE_MAX];
static uint8_t full_file[SFS_FILE_MAX];

static bool rd(void *ctx, uint32_t lba, uint8_t out[512])
{
    CHECK(ctx == medium && lba < SFS_DISK_SECTORS);
    if (lba == failing_read) return false;
    memcpy(out, medium + (size_t)lba * 512, 512);
    return true;
}
static bool wr(void *ctx, uint32_t lba, const uint8_t in[512])
{
    CHECK(ctx == medium && lba < SFS_DISK_SECTORS);
    ++writes;
    size_t off = (size_t)lba * 512;
    if (++ops == cutoff) {
        if (tear) memcpy(durable + off, in, 13);
        return false;
    }
    memcpy(medium + off, in, 512);
    return true;
}
static bool sync_disk(void *ctx)
{
    CHECK(ctx == medium);
    if (++ops == cutoff) return false;
    memcpy(durable, medium, DISK_BYTES);
    return true;
}
static const struct sfs_io io = {medium, SFS_DISK_SECTORS, rd, wr, sync_disk};

static void read_exact(unsigned i, const uint8_t *content, size_t size)
{
    CHECK(sfs_entry_at(&v, i) && sfs_entry_at(&v, i)->size == size);
    CHECK(sfs_read(&v, i, output, sizeof(output)) == SFS_OK);
    CHECK(!size || !memcmp(output, content, size));
}

static void valid_names(void)
{
    CHECK(sfs_name_valid("Работа.txt"));
    CHECK(sfs_name_valid("📁.bin"));
    CHECK(sfs_name_valid("12345678901234567890123456789012345678901234567"));
    CHECK(!sfs_name_valid("123456789012345678901234567890123456789012345678"));
    CHECK(!sfs_name_valid("../file") && !sfs_name_valid("a\\b") && !sfs_name_valid("a\nb"));
    CHECK(!sfs_name_valid(".") && !sfs_name_valid("..") && !sfs_name_valid(""));
    CHECK(!sfs_name_valid("\xc0\x80") && !sfs_name_valid("\xe2\x82") && !sfs_name_valid("\xed\xa0\x80"));
    puts("PASS UTF-8 names, byte bounds and path delimiter refusal");
}

static void bad_media(void)
{
    CHECK(sfs_mount(&v, &w, &io) == SFS_SIGNATURE && writes == 0);
    medium[DISK_BYTES - 1] = 1;
    CHECK(sfs_format(&v, &w, &io) == SFS_SIGNATURE && writes == 0);
    medium[DISK_BYTES - 1] = 0;
    medium[0] = 0x55;
    CHECK(sfs_mount(&v, &w, &io) == SFS_SIGNATURE && writes == 0);
    medium[0] = 0;
    struct sfs_io foreign = io;
    foreign.sectors -= 1;
    CHECK(sfs_mount(&v, &w, &foreign) == SFS_SIGNATURE && writes == 0);
    CHECK(sfs_format(&v, &w, &io) == SFS_OK);
    unsigned count = writes;
    CHECK(sfs_mount(&v, &w, &io) == SFS_OK && writes == count);
    CHECK(sfs_format(&v, &w, &io) == SFS_SIGNATURE && writes == count);
    CHECK(sfs_mount(&v, &w, &io) == SFS_OK);
    puts("PASS mount never writes; no formatting foreign, dirty or wrong-size media");
}

static void files(void)
{
    unsigned a, b, i, j;
    CHECK(sfs_mkdir(&v, &w, 0, "Документы", &a) == SFS_OK);
    CHECK(sfs_mkdir(&v, &w, a, "Подпапка", &b) == SFS_OK);
    CHECK(sfs_create(&v, &w, b, "данные.unknown", old_data, sizeof(old_data), &i) == SFS_OK);
    read_exact(i, old_data, sizeof(old_data));
    CHECK(sfs_mkdir(&v, &w, b, "данные.unknown", 0) == SFS_EXISTS);
    CHECK(sfs_create(&v, &w, i, "child", 0, 0, 0) == SFS_TYPE);
    CHECK(sfs_move(&v, &w, a, b, "cycle") == SFS_TYPE);
    CHECK(sfs_remove(&v, &w, b) == SFS_NOT_EMPTY);
    CHECK(sfs_copy(&v, &w, i, a, "копия.bin", &j) == SFS_OK);
    read_exact(j, old_data, sizeof(old_data));
    CHECK(sfs_move(&v, &w, i, a, "переименован.bin") == SFS_OK);
    CHECK(sfs_remove(&v, &w, b) == SFS_OK);
    CHECK(sfs_mount(&v, &w, &io) == SFS_OK);
    CHECK(sfs_find(&v, a, "переименован.bin") == (int)i && sfs_find(&v, a, "Подпапка") == -1);
    read_exact(i, old_data, sizeof(old_data));
    read_exact(j, old_data, sizeof(old_data));
    CHECK(sfs_write(&v, &w, i, new_data, sizeof(new_data)) == SFS_OK);
    read_exact(i, new_data, sizeof(new_data));
    read_exact(j, old_data, sizeof(old_data));
    CHECK(sfs_read(&v, i, output, 2) == SFS_RANGE);
    CHECK(sfs_remove(&v, &w, j) == SFS_OK);
    CHECK(sfs_create(&v, &w, a, "zero.txt", 0, 0, &j) == SFS_OK);
    read_exact(j, NULL, 0);
    CHECK(sfs_write(&v, &w, j, full_file, sizeof(full_file)) == SFS_OK);
    read_exact(j, full_file, sizeof(full_file));
    CHECK(sfs_write(&v, &w, j, full_file, sizeof(full_file) + 1) == SFS_RANGE);
    CHECK(sfs_remove(&v, &w, j) == SFS_OK);
    puts("PASS nested directories, file CRUD, copy, move, size limit and binary round-trip");
}

static void interruption(void)
{
    unsigned i;
    CHECK(sfs_create(&v, &w, 0, "crash.bin", old_data, sizeof(old_data), &i) == SFS_OK);
    memcpy(baseline, durable, DISK_BYTES);
    ops = 0;
    CHECK(sfs_write(&v, &w, i, new_data, sizeof(new_data)) == SFS_OK);
    uint32_t total = ops;
    CHECK(total == 16);
    for (uint32_t n = 1; n <= total; ++n) {
        memcpy(medium, baseline, DISK_BYTES);
        memcpy(durable, baseline, DISK_BYTES);
        cutoff = ops = 0;
        CHECK(sfs_mount(&v, &w, &io) == SFS_OK);
        cutoff = n;
        tear = true;
        CHECK(sfs_write(&v, &w, i, new_data, sizeof(new_data)) == SFS_IO && !v.mounted);
        CHECK(ops == cutoff);
        memcpy(medium, durable, DISK_BYTES);
        cutoff = ops = 0;
        unsigned before = writes;
        CHECK(sfs_mount(&v, &w, &io) == SFS_OK && before == writes);
        read_exact(i, old_data, sizeof(old_data));
    }
    tear = false;
    CHECK(sfs_write(&v, &w, i, new_data, sizeof(new_data)) == SFS_OK);
    CHECK(sfs_mount(&v, &w, &io) == SFS_OK);
    read_exact(i, new_data, sizeof(new_data));
    /* Corrupt the new payload, not the still valid previous bank. */
    unsigned bank = sfs_entry_at(&v, i)->bank;
    size_t lba = 19 + ((i - 1) * 2 + bank) * 128;
    medium[lba * 512] ^= 1;
    CHECK(sfs_mount(&v, &w, &io) == SFS_OK);
    read_exact(i, old_data, sizeof(old_data));
    puts("PASS 16 interruption points incl. torn commit; completed commit and corruption fallback");
}

static void readonly_and_limits(void)
{
    const struct sfs_io ro = {medium, SFS_DISK_SECTORS, rd, NULL, NULL};
    unsigned before = writes;
    failing_read = 1;
    CHECK(sfs_mount(&v, &w, &io) == SFS_IO && !v.mounted && before == writes);
    failing_read = UINT32_MAX;
    CHECK(sfs_mount(&v, &w, &ro) == SFS_OK);
    CHECK(sfs_mkdir(&v, &w, 0, "forbidden", NULL) == SFS_STATE && before == writes);
    CHECK(sfs_mount(&v, &w, &io) == SFS_OK);
    char field[16];
    unsigned n = 0;
    enum sfs_result r;
    do {
        snprintf(field, sizeof(field), "dir-%u", n++);
        r = sfs_mkdir(&v, &w, 0, field, 0);
    } while (r == SFS_OK && n <= SFS_ENTRIES);
    CHECK(r == SFS_FULL);
    CHECK(sfs_mount(&v, &w, &io) == SFS_OK);
    CHECK(sfs_mkdir(&v, &w, 0, "full", NULL) == SFS_FULL);
    puts("PASS read-only view, read failure, capacity and remount");
}

int main(void)
{
    for (unsigned i = 0; i < sizeof(old_data); ++i) old_data[i] = (uint8_t)(i * 13);
    for (unsigned i = 0; i < sizeof(new_data); ++i) new_data[i] = (uint8_t)(i * 53 + 9);
    for (unsigned i = 0; i < sizeof(full_file); ++i) full_file[i] = (uint8_t)(i * 3);
    valid_names(); bad_media(); files(); interruption(); readonly_and_limits();
    puts("STYLEFS HOST: PASS");
    return 0;
}
