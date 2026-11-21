/* Actual StyleFS v2 roundtrip for Editor and independent Notes filenames.
 * Build the same production source with section-garbage-collection on host. */
#include <styleos/desktop_stylefs.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum { SECTORS = SFS_V2_MIN_SECTORS };
static uint8_t *data;
static struct desk_fs browser;

static bool rd(void *context, uint32_t lba, uint8_t sector[512])
{
    (void)context;
    if (lba >= SECTORS) return false;
    memcpy(sector, data + (size_t)lba * 512, 512);
    return true;
}
static bool wr(void *context, uint32_t lba, const uint8_t sector[512])
{
    (void)context;
    if (lba >= SECTORS) return false;
    memcpy(data + (size_t)lba * 512, sector, 512);
    return true;
}
static bool sync(void *ctx) { (void)ctx; return true; }

int main(void)
{
    data = calloc(SECTORS, SFS_SECTOR_BYTES);
    assert(data);
    struct sfs_io io = {.context = 0, .sectors = SECTORS, .read = rd,
                        .write = wr, .flush = sync};
    assert(sfs_format(&browser.disk, &browser.work, &io) == SFS_OK);
    assert(browser.disk.mounted && browser.disk.format == 2);
    int note = -1, editor = -1;
    const char note_text[] = "заметка — отдельная\n";
    const char editor_text[] = "document text\n";
    assert(desk_fs_save_note(&browser, 0, &note, note_text, sizeof(note_text) - 1));
    assert(desk_fs_save_text(&browser, 0, &editor, editor_text, sizeof(editor_text) - 1));
    assert(note > 0 && editor > 0 && note != editor);
    assert(sfs_find(&browser.disk, 0, "Заметка.txt") == note);
    assert(sfs_find(&browser.disk, 0, "Текст.txt") == editor);
    char out[DESK_STYLEFS_TEXT_LIMIT] = {0};
    assert(sfs_read(&browser.disk, (unsigned)note, (uint8_t *)out, sizeof(out)) == SFS_OK);
    assert(memcmp(out, note_text, sizeof(note_text) - 1) == 0);
    assert(sfs_read(&browser.disk, (unsigned)editor, (uint8_t *)out, sizeof(out)) == SFS_OK);
    assert(memcmp(out, editor_text, sizeof(editor_text) - 1) == 0);
    const char replacement[] = "modified note";
    assert(desk_fs_save_note(&browser, 0, &note, replacement, sizeof(replacement) - 1));
    assert(sfs_find(&browser.disk, 0, "Заметка.txt") == note);
    int note2 = -1;
    assert(desk_fs_save_note(&browser, 0, &note2, note_text, sizeof(note_text) - 1));
    assert(note2 > 0 && note2 != note);
    assert(sfs_find(&browser.disk, 0, "Заметка 2.txt") == note2);
    struct sfs_volume mounted = {0};
    struct sfs_workspace scratch = {0};
    assert(sfs_mount(&mounted, &scratch, &io) == SFS_OK);
    assert(sfs_find(&mounted, 0, "Заметка.txt") == note);
    assert(sfs_find(&mounted, 0, "Заметка 2.txt") == note2);
    assert(sfs_find(&mounted, 0, "Текст.txt") == editor);
    assert(sfs_read(&mounted, (unsigned)note, (uint8_t *)out, sizeof(out)) == SFS_OK);
    assert(memcmp(out, replacement, sizeof(replacement) - 1) == 0);
    free(data);
    puts("NOTES STYLEFS V2 HOST PASS: independent names, no overwrite, update, remount");
    return 0;
}
