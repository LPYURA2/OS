#include <styleos/desktop_v0.h>
#include <styleos/arch.h>

/* A dedicated 2 MiB VM data image, not a partition or general filesystem.
 * Each named document slot has two independently checked generations. */
enum { SECTOR = 512, DATA_SECTORS = DESK_TEXT_LIMIT / SECTOR, BANK = 1 + DATA_SECTORS,
       DISK_SECTORS = 4096, VALID = 1, EMPTY = 0, BAD = -1, IO = -2 };
static const char super_id[] = "STYLEOS-DESK-V01";
static const char record_id[] = "STYLED01";

static bool bytes_are(const uint8_t *bytes, const char *magic, size_t size)
{
    for (size_t i = 0; i < size; ++i) if (bytes[i] != (uint8_t)magic[i]) return false;
    return true;
}
static bool zero(const uint8_t *bytes, size_t size)
{
    for (size_t i = 0; i < size; ++i) if (bytes[i]) return false;
    return true;
}
static uint32_t get32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}
static void put32(uint8_t *b, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) b[i] = (uint8_t)(v >> (i * 8));
}
static uint32_t hash(uint32_t h, const uint8_t *b, size_t size)
{
    for (size_t i = 0; i < size; ++i) h = (h ^ b[i]) * UINT32_C(16777619);
    return h;
}
static uint32_t start(unsigned slot, unsigned bank) { return 1 + (slot * 2 + bank) * BANK; }

static int read_bank(unsigned slot, unsigned bank, struct desk_record *r, char *out)
{
    uint8_t sec[SECTOR];
    uint32_t lba = start(slot, bank);
    if (!desk_ata_read(lba, sec)) return IO;
    if (zero(sec, sizeof(sec))) return EMPTY;
    if (!bytes_are(sec, record_id, sizeof(record_id) - 1) ||
        !zero(sec + 24, SECTOR - 24) || sec[16] > DESK_PROJECTS || sec[17] != slot || sec[18] || sec[19])
        return BAD;
    uint32_t gen = get32(sec + 8), size = get32(sec + 12), expected = get32(sec + 20);
    if (!gen || size > DESK_TEXT_LIMIT) return BAD;
    uint8_t folder = sec[16];
    uint32_t h = hash(UINT32_C(2166136261), sec, 20);
    for (unsigned off = 0; off < size; off += SECTOR) {
        if (!desk_ata_read(++lba, sec)) return IO;
        unsigned n = (size - off < SECTOR) ? size - off : SECTOR;
        h = hash(h, sec, n);
        if (out) for (unsigned i = 0; i < n; ++i) out[off + i] = (char)sec[i];
    }
    if (h != expected) return BAD;
    *r = (struct desk_record){gen, size, folder, (uint8_t)bank, false};
    return VALID;
}

void desk_store_mount(struct desk_store *s)
{
    *s = (struct desk_store){0};
    uint8_t sec[SECTOR];
    const size_t magic = sizeof(super_id) - 1;
    if (desk_ata_probe() != DISK_SECTORS) return;
    if (!desk_ata_read(0, sec)) { arch_puts("STYLEOS DESKTOP DATA SUPER READ ERROR\n"); return; }
    if (!bytes_are(sec, super_id, magic) || !zero(sec + magic, SECTOR - magic)) {
        arch_puts("STYLEOS DESKTOP DATA SUPER INVALID\n"); return;
    }
    s->writable = true;
    for (unsigned i = 0; i < DESK_DOCS; ++i) {
        struct desk_record a = {0}, b = {0};
        int ra = read_bank(i, 0, &a, 0), rb = read_bank(i, 1, &b, 0);
        if (ra == IO || rb == IO) {
            arch_puts("STYLEOS DESKTOP DATA BANK READ ERROR\n"); s->writable = false;
        }
        if (a.generation > b.generation) s->file[i] = a;
        else if (b.generation > a.generation) s->file[i] = b;
        else if (ra != EMPTY || rb != EMPTY) s->file[i].blocked = true;
    }
}

bool desk_store_load(const struct desk_store *s, unsigned slot, char *text, size_t *length)
{
    if (slot >= DESK_DOCS || !s->file[slot].generation || !text || !length) return false;
    struct desk_record r = {0};
    if (read_bank(slot, s->file[slot].active, &r, text) != VALID ||
        r.generation != s->file[slot].generation || r.size != s->file[slot].size ||
        r.folder != s->file[slot].folder) return false;
    *length = r.size;
    return true;
}

bool desk_store_save(struct desk_store *s, unsigned slot, unsigned folder,
                     const char *text, size_t size)
{
    if (!s->writable || slot >= DESK_DOCS || s->file[slot].blocked ||
        s->file[slot].generation == UINT32_MAX || folder > DESK_PROJECTS ||
        size > DESK_TEXT_LIMIT || (!text && size)) return false;
    unsigned bank = s->file[slot].generation ? (s->file[slot].active ^ 1u) : 0u;
    uint32_t gen = s->file[slot].generation + 1, lba = start(slot, bank);
    for (unsigned off = 0; off < size; off += SECTOR) {
        uint8_t sec[SECTOR] = {0};
        size_t n = size - off < SECTOR ? size - off : SECTOR;
        for (size_t i = 0; i < n; ++i) sec[i] = (uint8_t)text[off + i];
        if (!desk_ata_write(lba + 1 + off / SECTOR, sec)) goto fault;
    }
    if (!desk_ata_flush()) goto fault;
    uint8_t commit[SECTOR] = {0};
    for (size_t i = 0; i < sizeof(record_id) - 1; ++i) commit[i] = (uint8_t)record_id[i];
    put32(commit + 8, gen);
    put32(commit + 12, (uint32_t)size);
    commit[16] = (uint8_t)folder;
    commit[17] = (uint8_t)slot;
    uint32_t h = hash(UINT32_C(2166136261), commit, 20);
    put32(commit + 20, hash(h, (const uint8_t *)text, size));
    if (!desk_ata_write(lba, commit) || !desk_ata_flush()) goto fault;
    struct desk_record verified;
    if (read_bank(slot, bank, &verified, 0) != VALID || verified.generation != gen ||
        verified.size != size || verified.folder != folder) goto fault;
    s->file[slot] = verified;
    return true;
fault:
    s->writable = false;
    return false;
}
