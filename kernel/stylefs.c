#include <styleos/stylefs.h>

enum {
    ENTRY_BYTES = 64, TABLE_SECTORS = SFS_ENTRIES * ENTRY_BYTES / SFS_SECTOR_BYTES,
    FILE_SECTORS = SFS_FILE_MAX / SFS_SECTOR_BYTES, DATA_LBA = 1 + 2 * (1 + TABLE_SECTORS),
    SECTOR = SFS_SECTOR_BYTES
};
_Static_assert(DATA_LBA + 2 * (SFS_ENTRIES - 1) * FILE_SECTORS <= SFS_DISK_SECTORS,
               "StyleFS data slots outside the volume");
static const uint8_t super_magic[8] = {'S','T','Y','L','E','F','S','1'};
static const uint8_t head_magic[8] = {'S','F','S','C','M','T','0','1'};

static void zero(void *p, size_t n)
{ uint8_t *b = p; for (size_t i = 0; i < n; ++i) b[i] = 0; }
static void copy(void *d, const void *s, size_t n)
{ uint8_t *a = d; const uint8_t *b = s; for (size_t i = 0; i < n; ++i) a[i] = b[i]; }
static bool zeros(const uint8_t *p, size_t n)
{ for (size_t i = 0; i < n; ++i) if (p[i]) return false; return true; }
static bool equal(const uint8_t *p, const uint8_t *q, size_t n)
{ for (size_t i = 0; i < n; ++i) if (p[i] != q[i]) return false; return true; }
static bool same_name(const char *a, const char *b)
{ while (*a && *a == *b) { ++a; ++b; } return *a == *b; }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (unsigned b = 0; b < 8; ++b) c = (c >> 1) ^ ((c & 1) ? UINT32_C(0xedb88320) : 0);
    }
    return c;
}
static uint32_t checksum(const uint8_t *p, size_t n) { return ~crc(UINT32_MAX, p, n); }
static uint32_t header(unsigned bank) { return 1 + bank * (1 + TABLE_SECTORS); }
static uint32_t file_lba(unsigned id, unsigned bank)
{ return DATA_LBA + ((id - 1u) * 2 + bank) * FILE_SECTORS; }

/* Exact-byte, case-sensitive UTF-8 filenames. No normalization or escape syntax. */
bool sfs_name_valid(const char *name)
{
    if (!name || !name[0] || same_name(name, ".") || same_name(name, "..")) return false;
    const uint8_t *s = (const uint8_t *)name;
    size_t i = 0;
    while (i <= SFS_NAME_MAX && s[i]) {
        uint8_t first = s[i++];
        if (first < 0x80) {
            if (first < 32 || first == 127 || first == '/' || first == '\\') return false;
            continue;
        }
        unsigned n;
        uint8_t lo = 0x80, hi = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) n = 1;
        else if (first >= 0xe0 && first <= 0xef) {
            n = 2;
            if (first == 0xe0) lo = 0xa0;
            if (first == 0xed) hi = 0x9f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            n = 3;
            if (first == 0xf0) lo = 0x90;
            if (first == 0xf4) hi = 0x8f;
        } else return false;
        if (i + n > SFS_NAME_MAX || s[i] < lo || s[i] > hi) return false;
        ++i;
        while (--n) { if (s[i] < 0x80 || s[i] > 0xbf) return false; ++i; }
    }
    return i != 0 && i <= SFS_NAME_MAX;
}

static void encode(uint8_t *buf, const struct sfs_entry *entries)
{
    zero(buf, SFS_ENTRIES * ENTRY_BYTES);
    for (unsigned i = 0; i < SFS_ENTRIES; ++i) {
        const struct sfs_entry *e = &entries[i];
        uint8_t *p = buf + i * ENTRY_BYTES;
        put32(p, e->size);
        put32(p + 4, e->checksum);
        p[8] = (uint8_t)e->parent;
        p[9] = (uint8_t)(e->parent >> 8);
        p[10] = e->type;
        p[11] = e->bank;
        copy(p + 16, e->name, sizeof(e->name));
    }
}

static bool decode(struct sfs_entry *entries, const uint8_t *buf)
{
    for (unsigned i = 0; i < SFS_ENTRIES; ++i) {
        const uint8_t *p = buf + i * ENTRY_BYTES;
        struct sfs_entry *e = &entries[i];
        if (!zeros(p + 12, 4) || p[63]) return false;
        e->size = get32(p);
        e->checksum = get32(p + 4);
        e->parent = p[8] | (uint16_t)p[9] << 8;
        e->type = p[10];
        e->bank = p[11];
        copy(e->name, p + 16, sizeof(e->name));
        if (!i) {
            if (e->type != SFS_DIRECTORY || e->parent || !zeros(p + 16, 48)) return false;
        } else if (e->type == SFS_FREE) {
            if (!zeros(p, ENTRY_BYTES)) return false;
            continue;
        } else if (!sfs_name_valid(e->name) || e->parent >= SFS_ENTRIES || e->parent == i) return false;
        for (unsigned n = 0; n < sizeof(e->name); ++n)
            if (!e->name[n]) { if (!zeros(p + 16 + n, sizeof(e->name) - n)) return false; break; }
        if (e->type == SFS_DIRECTORY) {
            if (e->size || e->checksum || e->bank) return false;
        } else if (e->type != SFS_FILE || e->size > SFS_FILE_MAX || e->bank > 1 ||
                   (!e->size && e->checksum)) return false;
    }
    for (unsigned i = 1; i < SFS_ENTRIES; ++i) {
        if (!entries[i].type) continue;
        unsigned parent = entries[i].parent;
        for (unsigned hops = 0; ; ++hops) {
            if (parent >= SFS_ENTRIES || entries[parent].type != SFS_DIRECTORY ||
                hops >= SFS_ENTRIES || parent == i) return false;
            if (parent == SFS_ROOT) break;
            parent = entries[parent].parent;
        }
        for (unsigned j = 1; j < i; ++j)
            if (entries[j].type && entries[i].parent == entries[j].parent &&
                same_name(entries[i].name, entries[j].name)) return false;
    }
    return true;
}

static enum sfs_result payload(const struct sfs_io *io, const struct sfs_entry *e,
                              unsigned id, uint8_t *out)
{
    uint8_t sector[SECTOR];
    uint32_t c = UINT32_MAX;
    uint32_t lba = file_lba(id, e->bank);
    for (unsigned off = 0; off < e->size; off += SECTOR) {
        if (!io->read(io->context, lba++, sector)) return SFS_IO;
        size_t n = e->size - off < SECTOR ? e->size - off : SECTOR;
        c = crc(c, sector, n);
        if (out) copy(out + off, sector, n);
    }
    return ~c == e->checksum ? SFS_OK : SFS_CORRUPT;
}

static enum sfs_result read_commit(const struct sfs_io *io, struct sfs_workspace *w,
                                   unsigned bank, uint32_t *gen, uint32_t *digest)
{
    uint8_t *s = w->sector;
    if (!io->read(io->context, header(bank), s)) return SFS_IO;
    if (!equal(s, head_magic, 8) || !get32(s + 8) || get32(s + 16) != bank ||
        get32(s + 20) != checksum(s, 20) || !zeros(s + 24, SECTOR - 24)) return SFS_CORRUPT;
    *gen = get32(s + 8);
    *digest = get32(s + 12);
    return SFS_OK;
}

static enum sfs_result read_tree(const struct sfs_io *io, struct sfs_workspace *w,
                                 unsigned bank, uint32_t digest)
{
    for (unsigned i = 0; i < TABLE_SECTORS; ++i)
        if (!io->read(io->context, header(bank) + 1 + i, w->table + i * SECTOR)) return SFS_IO;
    if (checksum(w->table, sizeof(w->table)) != digest || !decode(w->pending, w->table)) return SFS_CORRUPT;
    for (unsigned i = 1; i < SFS_ENTRIES; ++i) if (w->pending[i].type == SFS_FILE) {
        enum sfs_result r = payload(io, &w->pending[i], i, 0);
        if (r != SFS_OK) return r;
    }
    return SFS_OK;
}

static void superblock(uint8_t *s)
{
    zero(s, SECTOR);
    copy(s, super_magic, sizeof(super_magic));
    put32(s + 8, 1);
    put32(s + 12, SECTOR);
    put32(s + 16, SFS_DISK_SECTORS);
    put32(s + 20, SFS_ENTRIES);
    put32(s + 24, DATA_LBA);
    put32(s + 28, FILE_SECTORS);
    put32(s + 32, checksum(s, 32));
}

enum sfs_result sfs_mount(struct sfs_volume *v, struct sfs_workspace *w, const struct sfs_io *source)
{
    if (!v || !w || !source) return SFS_STATE;
    struct sfs_io io = *source;
    v->mounted = false;
    if (io.sectors != SFS_DISK_SECTORS || !io.read) return SFS_SIGNATURE;
    if (!io.read(io.context, 0, w->sector)) return SFS_IO;
    if (!equal(w->sector, super_magic, sizeof(super_magic))) return SFS_SIGNATURE;
    uint8_t expect[SECTOR];
    superblock(expect);
    if (!equal(w->sector, expect, SECTOR)) return SFS_CORRUPT;
    uint32_t gen[2] = {0}, check[2] = {0};
    for (unsigned i = 0; i < 2; ++i)
        if (read_commit(&io, w, i, &gen[i], &check[i]) == SFS_IO) return SFS_IO;
    if (gen[0] == gen[1]) return SFS_CORRUPT;
    unsigned newest = gen[1] > gen[0];
    for (unsigned i = 0; i < 2; ++i) {
        unsigned bank = newest ^ i;
        if (!gen[bank]) continue;
        enum sfs_result r = read_tree(&io, w, bank, check[bank]);
        if (r == SFS_IO) return r;
        if (r != SFS_OK) continue;
        copy(v->entries, w->pending, sizeof(v->entries));
        v->io = io;
        v->generation = gen[bank];
        v->active = bank;
        v->mounted = true;
        return SFS_OK;
    }
    return SFS_CORRUPT;
}

static enum sfs_result failed_io(struct sfs_volume *v) { v->mounted = false; return SFS_IO; }
static enum sfs_result commit(struct sfs_volume *v, struct sfs_workspace *w)
{
    unsigned bank = v->active ^ 1u;
    uint32_t gen = v->generation + 1, lba = header(bank);
    const struct sfs_io *io = &v->io;
    encode(w->table, w->pending);
    for (unsigned i = 0; i < TABLE_SECTORS; ++i)
        if (!io->write(io->context, lba + 1 + i, w->table + i * SECTOR)) return failed_io(v);
    if (!io->flush(io->context)) return failed_io(v);
    zero(w->sector, SECTOR);
    copy(w->sector, head_magic, 8);
    put32(w->sector + 8, gen);
    put32(w->sector + 12, checksum(w->table, sizeof(w->table)));
    put32(w->sector + 16, bank);
    put32(w->sector + 20, checksum(w->sector, 20));
    if (!io->write(io->context, lba, w->sector) || !io->flush(io->context)) return failed_io(v);
    v->generation = gen;
    v->active = bank;
    copy(v->entries, w->pending, sizeof(v->entries));
    return SFS_OK;
}

#ifdef STYLEFS_HOST_FORMAT
enum sfs_result sfs_format(struct sfs_volume *v, struct sfs_workspace *w, const struct sfs_io *io)
{
    if (!v || !w || !io || !io->read || !io->write || !io->flush || io->sectors != SFS_DISK_SECTORS)
        return SFS_STATE;
    v->mounted = false;
    for (uint32_t i = 0; i < io->sectors; ++i) {
        if (!io->read(io->context, i, w->sector)) return SFS_IO;
        if (!zeros(w->sector, SECTOR)) return SFS_SIGNATURE;
    }
    v->io = *io;
    v->active = 1;
    v->generation = 0;
    zero(w->pending, sizeof(w->pending));
    w->pending[0].type = SFS_DIRECTORY;
    enum sfs_result r = commit(v, w);
    if (r != SFS_OK) return r;
    superblock(w->sector);
    if (!io->write(io->context, 0, w->sector) || !io->flush(io->context)) return failed_io(v);
    v->mounted = true;
    return SFS_OK;
}
#endif

const struct sfs_entry *sfs_entry_at(const struct sfs_volume *v, unsigned id)
{ return v && v->mounted && id < SFS_ENTRIES && v->entries[id].type ? &v->entries[id] : 0; }

int sfs_find(const struct sfs_volume *v, unsigned parent, const char *name)
{
    const struct sfs_entry *p = sfs_entry_at(v, parent);
    if (!p || p->type != SFS_DIRECTORY || !sfs_name_valid(name)) return -1;
    for (unsigned i = 1; i < SFS_ENTRIES; ++i)
        if (v->entries[i].type && v->entries[i].parent == parent && same_name(v->entries[i].name, name))
            return (int)i;
    return -1;
}

static bool can_write(const struct sfs_volume *v, const struct sfs_workspace *w)
{ return v && w && v->mounted && v->io.write && v->io.flush && v->generation != UINT32_MAX; }

static void copy_name(char *out, const char *name)
{ for (unsigned i = 0; name[i]; ++i) out[i] = name[i]; }

static enum sfs_result prepare_new(struct sfs_volume *v, struct sfs_workspace *w,
                                   unsigned parent, const char *name, uint8_t type, unsigned *id)
{
    if (!can_write(v, w)) return SFS_STATE;
    if (!sfs_name_valid(name)) return SFS_NAME;
    const struct sfs_entry *p = sfs_entry_at(v, parent);
    if (!p || p->type != SFS_DIRECTORY) return SFS_TYPE;
    if (sfs_find(v, parent, name) >= 0) return SFS_EXISTS;
    unsigned i = 1;
    while (i < SFS_ENTRIES && v->entries[i].type) ++i;
    if (i == SFS_ENTRIES) return SFS_FULL;
    copy(w->pending, v->entries, sizeof(v->entries));
    w->pending[i] = (struct sfs_entry){.parent = (uint16_t)parent, .type = type};
    copy_name(w->pending[i].name, name);
    *id = i;
    return SFS_OK;
}

static enum sfs_result write_data(struct sfs_volume *v, struct sfs_workspace *w, unsigned id,
                                  const uint8_t *data, size_t size, unsigned source)
{
    struct sfs_entry *e = &w->pending[id];
    uint32_t lba = file_lba(id, e->bank), from = 0, c = UINT32_MAX;
    const struct sfs_io *io = &v->io;
    if (source) from = file_lba(source, v->entries[source].bank);
    for (size_t off = 0; off < size; off += SECTOR) {
        size_t n = size - off < SECTOR ? size - off : SECTOR;
        if (source) {
            if (!io->read(io->context, from++, w->sector)) return failed_io(v);
            zero(w->sector + n, SECTOR - n);
        } else {
            zero(w->sector, SECTOR);
            copy(w->sector, data + off, n);
        }
        c = crc(c, w->sector, n);
        if (!io->write(io->context, lba++, w->sector)) return failed_io(v);
    }
    if (source && ~c != v->entries[source].checksum) { v->mounted = false; return SFS_CORRUPT; }
    if (size && !io->flush(io->context)) return failed_io(v);
    e->size = (uint32_t)size;
    e->checksum = ~c;
    return commit(v, w);
}

enum sfs_result sfs_mkdir(struct sfs_volume *v, struct sfs_workspace *w, unsigned parent,
                         const char *name, unsigned *id)
{
    unsigned n;
    enum sfs_result r = prepare_new(v, w, parent, name, SFS_DIRECTORY, &n);
    if (r == SFS_OK) r = commit(v, w);
    if (r == SFS_OK && id) *id = n;
    return r;
}

enum sfs_result sfs_create(struct sfs_volume *v, struct sfs_workspace *w, unsigned parent,
                          const char *name, const uint8_t *data, size_t size, unsigned *id)
{
    if (size > SFS_FILE_MAX || (size && !data)) return SFS_RANGE;
    unsigned n;
    enum sfs_result r = prepare_new(v, w, parent, name, SFS_FILE, &n);
    if (r == SFS_OK) r = write_data(v, w, n, data, size, 0);
    if (r == SFS_OK && id) *id = n;
    return r;
}

enum sfs_result sfs_read(const struct sfs_volume *v, unsigned id, uint8_t *data, size_t capacity)
{
    const struct sfs_entry *e = sfs_entry_at(v, id);
    if (!e) return SFS_NOT_FOUND;
    if (e->type != SFS_FILE) return SFS_TYPE;
    if (capacity < e->size || (e->size && !data)) return SFS_RANGE;
    return payload(&v->io, e, id, data);
}

enum sfs_result sfs_write(struct sfs_volume *v, struct sfs_workspace *w, unsigned id,
                         const uint8_t *data, size_t size)
{
    if (!can_write(v, w)) return SFS_STATE;
    if (size > SFS_FILE_MAX || (size && !data)) return SFS_RANGE;
    const struct sfs_entry *e = sfs_entry_at(v, id);
    if (!e) return SFS_NOT_FOUND;
    if (e->type != SFS_FILE) return SFS_TYPE;
    copy(w->pending, v->entries, sizeof(v->entries));
    w->pending[id].bank ^= 1;
    return write_data(v, w, id, data, size, 0);
}

enum sfs_result sfs_move(struct sfs_volume *v, struct sfs_workspace *w, unsigned id,
                        unsigned parent, const char *name)
{
    if (!can_write(v, w)) return SFS_STATE;
    const struct sfs_entry *e = sfs_entry_at(v, id), *p = sfs_entry_at(v, parent);
    if (!e || id == SFS_ROOT) return SFS_NOT_FOUND;
    if (!p || p->type != SFS_DIRECTORY) return SFS_TYPE;
    if (!sfs_name_valid(name)) return SFS_NAME;
    int existing = sfs_find(v, parent, name);
    if (existing >= 0) return (unsigned)existing == id ? SFS_OK : SFS_EXISTS;
    for (unsigned i = parent; i; i = v->entries[i].parent) if (i == id) return SFS_TYPE;
    copy(w->pending, v->entries, sizeof(v->entries));
    w->pending[id].parent = (uint16_t)parent;
    zero(w->pending[id].name, sizeof(e->name));
    copy_name(w->pending[id].name, name);
    return commit(v, w);
}

enum sfs_result sfs_copy(struct sfs_volume *v, struct sfs_workspace *w, unsigned id,
                        unsigned parent, const char *name, unsigned *new_id)
{
    const struct sfs_entry *e = sfs_entry_at(v, id);
    if (!e) return SFS_NOT_FOUND;
    if (e->type != SFS_FILE) return SFS_TYPE;
    unsigned n;
    enum sfs_result r = prepare_new(v, w, parent, name, SFS_FILE, &n);
    if (r == SFS_OK) r = write_data(v, w, n, 0, e->size, id);
    if (r == SFS_OK && new_id) *new_id = n;
    return r;
}

enum sfs_result sfs_remove(struct sfs_volume *v, struct sfs_workspace *w, unsigned id)
{
    if (!can_write(v, w)) return SFS_STATE;
    if (!id || !sfs_entry_at(v, id)) return SFS_NOT_FOUND;
    for (unsigned i = 1; i < SFS_ENTRIES; ++i)
        if (v->entries[i].type && v->entries[i].parent == id) return SFS_NOT_EMPTY;
    copy(w->pending, v->entries, sizeof(v->entries));
    zero(&w->pending[id], sizeof(w->pending[id]));
    return commit(v, w);
}

const char *sfs_result_name(enum sfs_result r)
{
    static const char *const names[] = {"ok", "io", "signature", "corrupt", "state", "name",
        "not-found", "exists", "type", "full", "range", "not-empty"};
    return (unsigned)r < sizeof(names) / sizeof(*names) ? names[r] : "unknown";
}
