#include <styleos/desktop_v0.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t disk[73][512];
static unsigned writes, flushes, capacity = 4096, fail_write;
static char retrieved[DESK_TEXT_LIMIT];

void arch_puts(const char *s) { (void)s; }
uint32_t desk_ata_probe(void) { return capacity; }
bool desk_ata_read(uint32_t lba, uint8_t data[512])
{
    assert(lba < 73);
    memcpy(data, disk[lba], 512);
    return true;
}
bool desk_ata_write(uint32_t lba, const uint8_t data[512])
{
    assert(lba > 0 && lba < 73);
    if (++writes == fail_write) return false;
    memcpy(disk[lba], data, 512);
    return true;
}
bool desk_ata_flush(void) { ++flushes; return true; }

static void compare(const struct desk_store *s, const char *text)
{
    size_t n;
    assert(desk_store_load(s, 0, retrieved, &n));
    assert(n == strlen(text) && memcmp(text, retrieved, n) == 0);
}

int main(void)
{
    struct desk_store s;
    desk_store_mount(&s);
    assert(!s.writable && !desk_store_save(&s, 0, DESK_ROOT, "x", 1) && !writes);
    memcpy(disk[0], "STYLEOS-DESK-V01", 16);
    capacity = 4097;
    desk_store_mount(&s);
    assert(!s.writable && !writes);
    capacity = 4096;
    desk_store_mount(&s);
    assert(s.writable && !s.file[0].generation);
    assert(!desk_store_save(&s, 0, DESK_ROOT, "x", DESK_TEXT_LIMIT + 1));
    assert(!desk_store_save(&s, DESK_DOCS, DESK_ROOT, "x", 1));
    assert(!desk_store_save(&s, 0, DESK_PROJECTS + 1, "x", 1) && !writes);
    puts("PASS format, size, slot and folder bounds");

    assert(desk_store_save(&s, 0, DESK_PROJECTS, "alpha", 5));
    assert(writes == 2 && flushes == 2 && s.file[0].active == 0 && s.file[0].generation == 1);
    assert(desk_store_save(&s, 0, DESK_PROJECTS, "beta", 4));
    assert(writes == 4 && flushes == 4 && s.file[0].active == 1 && s.file[0].generation == 2);
    desk_store_mount(&s);
    assert(s.file[0].folder == DESK_PROJECTS);
    compare(&s, "beta");
    puts("PASS latest valid generation after remount");

    disk[11][0] ^= 1; /* newest payload, not its header */
    desk_store_mount(&s);
    compare(&s, "alpha");
    puts("PASS damaged new payload returns to previous generation");

    fail_write = writes + 2; /* interrupt the inactive generation before its commit */
    assert(!desk_store_save(&s, 0, DESK_PROJECTS, "gamma", 5));
    assert(!s.writable);
    desk_store_mount(&s);
    compare(&s, "alpha");
    puts("PASS interrupted update never replaces committed text");

    disk[2][0] ^= 1;
    desk_store_mount(&s);
    assert(s.file[0].blocked && !desk_store_save(&s, 0, DESK_PROJECTS, "theta", 5));
    puts("PASS two corrupt banks cannot be silently overwritten");
}
