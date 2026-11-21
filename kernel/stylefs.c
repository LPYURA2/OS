#include <styleos/stylefs.h>

enum {
    ENTRY_V1 = 64, ENTRY_V2 = 128,
    TABLE_V1 = SFS_V1_ENTRIES * ENTRY_V1 / SFS_SECTOR_BYTES,
    TABLE_V2 = SFS_ENTRIES * ENTRY_V2 / SFS_SECTOR_BYTES,
    FILE_SECTORS = SFS_FILE_MAX / SFS_SECTOR_BYTES,
    DATA_V1 = 1 + 2 * (1 + TABLE_V1), DATA_V2 = 1 + 2 * (1 + TABLE_V2),
    SECTOR = SFS_SECTOR_BYTES
};
_Static_assert(DATA_V1 + 2 * (SFS_V1_ENTRIES - 1) * FILE_SECTORS <= SFS_DISK_SECTORS,
               "StyleFS data slots outside the volume");
static const uint8_t super_v1[8] = {'S','T','Y','L','E','F','S','1'};
static const uint8_t super_v2[8] = {'S','T','Y','L','E','F','S','2'};
static const uint8_t head_v1[8] = {'S','F','S','C','M','T','0','1'};
static const uint8_t head_v2[8] = {'S','F','S','C','M','T','0','2'};

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
static unsigned slots(unsigned fmt) { return fmt == 2 ? SFS_ENTRIES : SFS_V1_ENTRIES; }
static unsigned table_sectors(unsigned fmt) { return fmt == 2 ? TABLE_V2 : TABLE_V1; }
static uint32_t header(unsigned fmt, unsigned bank) { return 1 + bank * (1 + table_sectors(fmt)); }
static uint32_t file_lba(unsigned fmt, unsigned id, const struct sfs_entry *e)
{ return fmt == 2 ? e->start : DATA_V1 + ((id - 1u) * 2 + e->bank) * FILE_SECTORS; }
static uint32_t used_sectors(uint32_t bytes) { return bytes / SECTOR + !!(bytes % SECTOR); }
bool sfs_capacity_valid(uint32_t sectors)
{
    return sectors == SFS_DISK_SECTORS ||
           (sectors >= SFS_V2_MIN_SECTORS && sectors <= SFS_V2_MAX_SECTORS && sectors % 2048 == 0);
}
unsigned sfs_name_limit(const struct sfs_volume *v) { return v && v->format == 2 ? SFS_NAME_MAX : SFS_V1_NAME_MAX; }
uint32_t sfs_file_limit(const struct sfs_volume *v) { return v && v->format == 2 ? SFS_V2_FILE_MAX : SFS_FILE_MAX; }

/* Exact-byte, case-sensitive UTF-8 filenames. No normalization or escape syntax. */
static bool name_valid(const char *name, unsigned limit)
{
    if (!name || !name[0] || same_name(name, ".") || same_name(name, "..")) return false;
    const uint8_t *s = (const uint8_t *)name;
    size_t i = 0;
    while (i <= limit && s[i]) {
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
        if (i + n > limit || s[i] < lo || s[i] > hi) return false;
        ++i;
        while (--n) { if (s[i] < 0x80 || s[i] > 0xbf) return false; ++i; }
    }
    return i != 0 && i <= limit;
}
bool sfs_name_valid(const char *name) { return name_valid(name, SFS_NAME_MAX); }

static void encode(uint8_t *buf, const struct sfs_entry *entries, unsigned fmt)
{
    unsigned stride = fmt == 2 ? ENTRY_V2 : ENTRY_V1;
    zero(buf, slots(fmt) * stride);
    for (unsigned i = 0; i < slots(fmt); ++i) {
        const struct sfs_entry *e = &entries[i];
        uint8_t *p = buf + i * stride;
        put32(p, e->size);
        put32(p + 4, e->checksum);
        p[8] = (uint8_t)e->parent;
        p[9] = (uint8_t)(e->parent >> 8);
        p[10] = e->type;
        p[11] = e->bank;
        if (fmt == 2) put32(p + 12, e->start);
        copy(p + 16, e->name, stride - 16);
    }
}

static bool decode(struct sfs_entry *entries, const uint8_t *buf, unsigned fmt, uint32_t capacity)
{
    unsigned stride = fmt == 2 ? ENTRY_V2 : ENTRY_V1, count = slots(fmt);
    unsigned name_size = stride - 16;
    zero(entries, sizeof(*entries) * SFS_ENTRIES);
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *p = buf + i * stride;
        struct sfs_entry *e = &entries[i];
        if ((fmt == 1 && !zeros(p + 12, 4)) || p[stride - 1]) return false;
        e->size = get32(p);
        e->checksum = get32(p + 4);
        e->parent = p[8] | (uint16_t)p[9] << 8;
        e->type = p[10];
        e->bank = p[11];
        e->start = get32(p + 12);
        copy(e->name, p + 16, name_size);
        if (!i) {
            if (e->type != SFS_DIRECTORY || e->parent || !zeros(p + 16, name_size)) return false;
        } else if (e->type == SFS_FREE) {
            if (!zeros(p, stride)) return false;
            continue;
        } else if (!name_valid(e->name, name_size - 1) || e->parent >= count || e->parent == i) return false;
        for (unsigned n = 0; n < name_size; ++n)
            if (!e->name[n]) { if (!zeros(p + 16 + n, name_size - n)) return false; break; }
        if (e->type == SFS_DIRECTORY) {
            if (e->size || e->checksum || e->bank || e->start) return false;
        } else if (e->type != SFS_FILE || (!e->size && e->checksum)) return false;
        else if (fmt == 1) {
            if (e->size > SFS_FILE_MAX || e->bank > 1) return false;
        } else {
            if (e->bank || e->size > SFS_V2_FILE_MAX) return false;
            if (e->size) {
                if (e->start < DATA_V2 || e->start >= capacity || used_sectors(e->size) > capacity - e->start)
                    return false;
            } else if (e->start) return false;
        }
    }
    for (unsigned i = 1; i < count; ++i) {
        if (!entries[i].type) continue;
        unsigned parent = entries[i].parent;
        for (unsigned hops = 0; ; ++hops) {
            if (parent >= count || entries[parent].type != SFS_DIRECTORY ||
                hops >= count || parent == i) return false;
            if (parent == SFS_ROOT) break;
            parent = entries[parent].parent;
        }
        for (unsigned j = 1; j < i; ++j) {
            if (entries[j].type && entries[i].parent == entries[j].parent &&
                same_name(entries[i].name, entries[j].name)) return false;
            const struct sfs_entry *a = &entries[i], *b = &entries[j];
            if (fmt == 2 && a->size && b->size && a->start < b->start + used_sectors(b->size) &&
                b->start < a->start + used_sectors(a->size)) return false;
        }
    }
    return true;
}

static enum sfs_result payload(const struct sfs_io *io, const struct sfs_entry *e,
                              unsigned fmt, unsigned id, uint8_t *out, sfs_sink sink, void *ctx)
{
    uint8_t sector[SECTOR];
    uint32_t c = UINT32_MAX;
    uint32_t lba = file_lba(fmt, id, e);
    for (unsigned off = 0; off < e->size; off += SECTOR) {
        if (!io->read(io->context, lba++, sector)) return SFS_IO;
        size_t n = e->size - off < SECTOR ? e->size - off : SECTOR;
        c = crc(c, sector, n);
        if (out) copy(out + off, sector, n);
        if (sink && !sink(ctx, off, sector, n)) return SFS_IO;
    }
    return ~c == e->checksum ? SFS_OK : SFS_CORRUPT;
}

static enum sfs_result read_commit(const struct sfs_io *io, struct sfs_workspace *w,
                                   unsigned fmt, unsigned bank, uint32_t *gen, uint32_t *digest)
{
    uint8_t *s = w->sector;
    if (!io->read(io->context, header(fmt, bank), s)) return SFS_IO;
    if (!equal(s, fmt == 2 ? head_v2 : head_v1, 8) || !get32(s + 8) || get32(s + 16) != bank ||
        get32(s + 20) != checksum(s, 20) || !zeros(s + 24, SECTOR - 24)) return SFS_CORRUPT;
    *gen = get32(s + 8);
    *digest = get32(s + 12);
    return SFS_OK;
}

static enum sfs_result read_tree(const struct sfs_io *io, struct sfs_workspace *w,
                                 unsigned fmt, unsigned bank, uint32_t digest)
{
    for (unsigned i = 0; i < table_sectors(fmt); ++i)
        if (!io->read(io->context, header(fmt, bank) + 1 + i, w->table + i * SECTOR)) return SFS_IO;
    if (checksum(w->table, table_sectors(fmt) * SECTOR) != digest ||
        !decode(w->pending, w->table, fmt, io->sectors)) return SFS_CORRUPT;
    /* Preserve v1 recovery. v2 checks content when accessed, not by scanning the disk at mount. */
    for (unsigned i = 1; fmt == 1 && i < SFS_V1_ENTRIES; ++i) if (w->pending[i].type == SFS_FILE) {
        enum sfs_result r = payload(io, &w->pending[i], fmt, i, 0, 0, 0);
        if (r != SFS_OK) return r;
    }
    return SFS_OK;
}

static void superblock(uint8_t *s, unsigned fmt, uint32_t capacity)
{
    zero(s, SECTOR);
    copy(s, fmt == 2 ? super_v2 : super_v1, 8);
    put32(s + 8, fmt);
    put32(s + 12, SECTOR);
    put32(s + 16, capacity);
    put32(s + 20, slots(fmt));
    put32(s + 24, fmt == 2 ? DATA_V2 : DATA_V1);
    put32(s + 28, fmt == 2 ? ENTRY_V2 : FILE_SECTORS);
    put32(s + 32, checksum(s, 32));
}

enum sfs_result sfs_mount(struct sfs_volume *v, struct sfs_workspace *w, const struct sfs_io *source)
{
    if (!v || !w || !source) return SFS_STATE;
    struct sfs_io io = *source;
    v->mounted = false;
    if (!sfs_capacity_valid(io.sectors) || !io.read) return SFS_SIGNATURE;
    unsigned fmt = io.sectors == SFS_DISK_SECTORS ? 1 : 2;
    if (!io.read(io.context, 0, w->sector)) return SFS_IO;
    if (!equal(w->sector, fmt == 2 ? super_v2 : super_v1, 8)) return SFS_SIGNATURE;
    uint8_t expect[SECTOR];
    superblock(expect, fmt, io.sectors);
    if (!equal(w->sector, expect, SECTOR)) return SFS_CORRUPT;
    uint32_t gen[2] = {0}, check[2] = {0};
    for (unsigned i = 0; i < 2; ++i)
        if (read_commit(&io, w, fmt, i, &gen[i], &check[i]) == SFS_IO) return SFS_IO;
    if (gen[0] == gen[1]) return SFS_CORRUPT;
    unsigned newest = gen[1] > gen[0];
    for (unsigned i = 0; i < 2; ++i) {
        unsigned bank = newest ^ i;
        if (!gen[bank]) continue;
        enum sfs_result r = read_tree(&io, w, fmt, bank, check[bank]);
        if (r == SFS_IO) return r;
        if (r != SFS_OK) continue;
        copy(v->entries, w->pending, sizeof(v->entries));
        zero(v->previous, sizeof(v->previous));
        if (fmt == 2 && i == 0 && gen[bank ^ 1u]) {
            r = read_tree(&io, w, fmt, bank ^ 1u, check[bank ^ 1u]);
            if (r == SFS_IO) return r;
            if (r == SFS_OK) copy(v->previous, w->pending, sizeof(v->previous));
        }
        v->io = io;
        v->generation = gen[bank];
        v->active = bank;
        v->format = fmt;
        v->mounted = true;
        return SFS_OK;
    }
    return SFS_CORRUPT;
}

static enum sfs_result failed_io(struct sfs_volume *v) { v->mounted = false; return SFS_IO; }
static enum sfs_result commit(struct sfs_volume *v, struct sfs_workspace *w)
{
    unsigned bank = v->active ^ 1u;
    unsigned fmt = v->format;
    uint32_t gen = v->generation + 1, lba = header(fmt, bank);
    const struct sfs_io *io = &v->io;
    encode(w->table, w->pending, fmt);
    for (unsigned i = 0; i < table_sectors(fmt); ++i)
        if (!io->write(io->context, lba + 1 + i, w->table + i * SECTOR)) return failed_io(v);
    if (!io->flush(io->context)) return failed_io(v);
    zero(w->sector, SECTOR);
    copy(w->sector, fmt == 2 ? head_v2 : head_v1, 8);
    put32(w->sector + 8, gen);
    put32(w->sector + 12, checksum(w->table, table_sectors(fmt) * SECTOR));
    put32(w->sector + 16, bank);
    put32(w->sector + 20, checksum(w->sector, 20));
    if (!io->write(io->context, lba, w->sector) || !io->flush(io->context)) return failed_io(v);
    v->generation = gen;
    v->active = bank;
    if (fmt == 2) copy(v->previous, v->entries, sizeof(v->previous));
    copy(v->entries, w->pending, sizeof(v->entries));
    return SFS_OK;
}

#ifdef STYLEFS_HOST_FORMAT
enum sfs_result sfs_format(struct sfs_volume *v, struct sfs_workspace *w, const struct sfs_io *io)
{
    if (!v || !w || !io || !io->read || !io->write || !io->flush || !sfs_capacity_valid(io->sectors))
        return SFS_STATE;
    v->mounted = false;
    for (uint32_t i = 0; i < io->sectors; ++i) {
        if (!io->read(io->context, i, w->sector)) return SFS_IO;
        if (!zeros(w->sector, SECTOR)) return SFS_SIGNATURE;
    }
    v->io = *io;
    v->format = io->sectors == SFS_DISK_SECTORS ? 1 : 2;
    v->active = 1;
    v->generation = 0;
    zero(v->entries, sizeof(v->entries));
    zero(v->previous, sizeof(v->previous));
    zero(w->pending, sizeof(w->pending));
    w->pending[0].type = SFS_DIRECTORY;
    enum sfs_result r = commit(v, w);
    if (r != SFS_OK) return r;
    superblock(w->sector, v->format, io->sectors);
    if (!io->write(io->context, 0, w->sector) || !io->flush(io->context)) return failed_io(v);
    v->mounted = true;
    return SFS_OK;
}
#endif

const struct sfs_entry *sfs_entry_at(const struct sfs_volume *v, unsigned id)
{ return v && v->mounted && id < slots(v->format) && v->entries[id].type ? &v->entries[id] : 0; }

int sfs_find(const struct sfs_volume *v, unsigned parent, const char *name)
{
    const struct sfs_entry *p = sfs_entry_at(v, parent);
    if (!p || p->type != SFS_DIRECTORY || !name_valid(name, sfs_name_limit(v))) return -1;
    for (unsigned i = 1; i < slots(v->format); ++i)
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
    if (!name_valid(name, sfs_name_limit(v))) return SFS_NAME;
    const struct sfs_entry *p = sfs_entry_at(v, parent);
    if (!p || p->type != SFS_DIRECTORY) return SFS_TYPE;
    if (sfs_find(v, parent, name) >= 0) return SFS_EXISTS;
    unsigned i = 1;
    while (i < slots(v->format) && v->entries[i].type) ++i;
    if (i == slots(v->format)) return SFS_FULL;
    copy(w->pending, v->entries, sizeof(v->entries));
    w->pending[i] = (struct sfs_entry){.parent = (uint16_t)parent, .type = type};
    copy_name(w->pending[i].name, name);
    *id = i;
    return SFS_OK;
}

/* A candidate never overlaps any extent of either admitted snapshot. One
 * contiguous extent per file keeps the parser and commit protocol bounded. */
static uint32_t reserve(const struct sfs_volume *v, uint32_t need)
{
    for (uint32_t start = DATA_V2; start < v->io.sectors && need <= v->io.sectors - start;) {
        uint32_t skip = start, end = start + need;
        const struct sfs_entry *trees[] = {v->entries, v->previous};
        for (unsigned t = 0; t < 2; ++t) for (unsigned i = 1; i < SFS_ENTRIES; ++i) {
            const struct sfs_entry *e = &trees[t][i];
            if (!e->size || e->start >= end) continue;
            uint32_t tail = e->start + used_sectors(e->size);
            if (tail > skip) skip = tail;
        }
        if (skip == start) return start;
        start = skip;
    }
    return 0;
}

static enum sfs_result write_data(struct sfs_volume *v, struct sfs_workspace *w, unsigned id,
                                  const uint8_t *data, size_t size, unsigned source,
                                  sfs_source stream, void *context)
{
    struct sfs_entry *e = &w->pending[id];
    if (v->format == 2) {
        e->start = size ? reserve(v, used_sectors((uint32_t)size)) : 0;
        if (size && !e->start) return SFS_FULL;
    }
    uint32_t lba = file_lba(v->format, id, e), from = 0, c = UINT32_MAX;
    const struct sfs_io *io = &v->io;
    if (source) from = file_lba(v->format, source, &v->entries[source]);
    for (size_t off = 0; off < size; off += SECTOR) {
        size_t n = size - off < SECTOR ? size - off : SECTOR;
        if (source) {
            if (!io->read(io->context, from++, w->sector)) return failed_io(v);
            zero(w->sector + n, SECTOR - n);
        } else if (stream) {
            if (!stream(context, (uint32_t)off, w->sector, n)) return failed_io(v);
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
    if (size > sfs_file_limit(v) || (size && !data)) return SFS_RANGE;
    unsigned n;
    enum sfs_result r = prepare_new(v, w, parent, name, SFS_FILE, &n);
    if (r == SFS_OK) r = write_data(v, w, n, data, size, 0, 0, 0);
    if (r == SFS_OK && id) *id = n;
    return r;
}

enum sfs_result sfs_create_from(struct sfs_volume *v, struct sfs_workspace *w, unsigned parent,
                               const char *name, uint32_t size, sfs_source stream, void *ctx, unsigned *id)
{
    if (size > sfs_file_limit(v) || (size && !stream)) return SFS_RANGE;
    unsigned n;
    enum sfs_result r = prepare_new(v, w, parent, name, SFS_FILE, &n);
    if (r == SFS_OK) r = write_data(v, w, n, 0, size, 0, stream, ctx);
    if (r == SFS_OK && id) *id = n;
    return r;
}

enum sfs_result sfs_read(const struct sfs_volume *v, unsigned id, uint8_t *data, size_t capacity)
{
    const struct sfs_entry *e = sfs_entry_at(v, id);
    if (!e) return SFS_NOT_FOUND;
    if (e->type != SFS_FILE) return SFS_TYPE;
    if (capacity < e->size || (e->size && !data)) return SFS_RANGE;
    return payload(&v->io, e, v->format, id, data, 0, 0);
}

enum sfs_result sfs_read_to(const struct sfs_volume *v, unsigned id, sfs_sink output, void *ctx)
{
    const struct sfs_entry *e = sfs_entry_at(v, id);
    if (!e) return SFS_NOT_FOUND;
    if (e->type != SFS_FILE) return SFS_TYPE;
    return payload(&v->io, e, v->format, id, 0, output, ctx);
}

enum sfs_result sfs_write(struct sfs_volume *v, struct sfs_workspace *w, unsigned id,
                         const uint8_t *data, size_t size)
{
    if (!can_write(v, w)) return SFS_STATE;
    if (size > sfs_file_limit(v) || (size && !data)) return SFS_RANGE;
    const struct sfs_entry *e = sfs_entry_at(v, id);
    if (!e) return SFS_NOT_FOUND;
    if (e->type != SFS_FILE) return SFS_TYPE;
    copy(w->pending, v->entries, sizeof(v->entries));
    if (v->format == 1) w->pending[id].bank ^= 1;
    return write_data(v, w, id, data, size, 0, 0, 0);
}

enum sfs_result sfs_move(struct sfs_volume *v, struct sfs_workspace *w, unsigned id,
                        unsigned parent, const char *name)
{
    if (!can_write(v, w)) return SFS_STATE;
    const struct sfs_entry *e = sfs_entry_at(v, id), *p = sfs_entry_at(v, parent);
    if (!e || id == SFS_ROOT) return SFS_NOT_FOUND;
    if (!p || p->type != SFS_DIRECTORY) return SFS_TYPE;
    if (!name_valid(name, sfs_name_limit(v))) return SFS_NAME;
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
    if (r == SFS_OK) r = write_data(v, w, n, 0, e->size, id, 0, 0);
    if (r == SFS_OK && new_id) *new_id = n;
    return r;
}

/* A copy job never changes the target table before a verified finish.
 * A cancelled job leaves only unreachable data sectors. The caller must keep
 * the source, destination and workspace exclusive until the job is inactive.
 * A failed write/flush makes the destination require a fresh mount. */
enum sfs_result sfs_copy_job_begin(struct sfs_copy_job *job, struct sfs_volume *source,
                                   unsigned source_id, struct sfs_volume *destination,
                                   struct sfs_workspace *w, unsigned target_folder,
                                   const char *target_name)
{
    if (!job || job->active || !source || !destination || source == destination ||
        source->format != 2 || destination->format != 2 ||
        !source->mounted || !destination->mounted || !source->io.read ||
        !w || !can_write(destination, w)) return SFS_STATE;
    const struct sfs_entry *entry = sfs_entry_at(source, source_id);
    if (!entry) return SFS_NOT_FOUND;
    if (entry->type != SFS_FILE) return SFS_TYPE;
    if (entry->size > sfs_file_limit(destination)) return SFS_RANGE;
    unsigned target_id;
    enum sfs_result r = prepare_new(destination, w, target_folder, target_name,
                                    SFS_FILE, &target_id);
    if (r != SFS_OK) return r;
    uint32_t destination_lba = entry->size ? reserve(destination, used_sectors(entry->size)) : 0;
    if (entry->size && !destination_lba) return SFS_FULL;
    w->pending[target_id].size = entry->size;
    w->pending[target_id].start = destination_lba;
    w->pending[target_id].checksum = entry->checksum;
    *job = (struct sfs_copy_job){
        .source = source, .destination = destination, .workspace = w,
        .source_generation = source->generation,
        .destination_generation = destination->generation,
        .source_lba = entry->start, .destination_lba = destination_lba,
        .size = entry->size, .copied = 0,
        .source_checksum = entry->checksum, .running_crc = UINT32_MAX,
        .target_id = target_id, .active = true
    };
    return SFS_OK;
}

void sfs_copy_job_cancel(struct sfs_copy_job *job)
{
    if (job) job->active = false;
}

enum sfs_result sfs_copy_job_step(struct sfs_copy_job *job, unsigned max_sectors,
                                  bool *completed)
{
    if (completed) *completed = false;
    if (!job || !job->active || !completed || !max_sectors) return SFS_STATE;
    struct sfs_volume *source = job->source, *destination = job->destination;
    struct sfs_workspace *w = job->workspace;
    if (!source || !destination || !w || !source->mounted || !destination->mounted ||
        source->generation != job->source_generation ||
        destination->generation != job->destination_generation) {
        sfs_copy_job_cancel(job);
        return SFS_STATE;
    }
    for (unsigned i = 0; i < max_sectors && job->copied < job->size; ++i) {
        uint32_t at = job->copied / SECTOR;
        size_t n = job->size - job->copied < SECTOR ? job->size - job->copied : SECTOR;
        if (!source->io.read(source->io.context, job->source_lba + at, w->sector)) {
            /* A vanished source requires re-mount before reuse. */
            source->mounted = false;
            sfs_copy_job_cancel(job);
            return SFS_IO;
        }
        job->running_crc = crc(job->running_crc, w->sector, n);
        zero(w->sector + n, SECTOR - n);
        if (!destination->io.write(destination->io.context,
                                   job->destination_lba + at, w->sector)) {
            sfs_copy_job_cancel(job);
            return failed_io(destination);
        }
        job->copied += (uint32_t)n;
    }
    if (job->copied < job->size) return SFS_OK;
    if (~job->running_crc != job->source_checksum) {
        sfs_copy_job_cancel(job);
        return SFS_CORRUPT;
    }
    if (!destination->io.flush(destination->io.context)) {
        sfs_copy_job_cancel(job);
        return failed_io(destination);
    }
    enum sfs_result r = commit(destination, w);
    sfs_copy_job_cancel(job);
    if (r == SFS_OK) *completed = true;
    /* On metadata commit I/O uncertainty, remount before deciding whether the
     * target was published. Do not delete its source or auto-retry a move. */
    return r;
}

enum sfs_result sfs_remove(struct sfs_volume *v, struct sfs_workspace *w, unsigned id)
{
    if (!can_write(v, w)) return SFS_STATE;
    if (!id || !sfs_entry_at(v, id)) return SFS_NOT_FOUND;
    for (unsigned i = 1; i < slots(v->format); ++i)
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
