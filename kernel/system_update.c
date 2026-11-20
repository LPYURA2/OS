#include <styleos/system_update.h>
#include "monocypher-ed25519.h"

enum { SIGNED_BYTES = 88, SIGNATURE_END = 152 };
_Static_assert(UP_SLOT_B + UP_SLOT_SECTORS <= UP_DISK_SECTORS - 33, "update slots overlap the backup GPT");

static uint32_t get32(const uint8_t *s)
{ return (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24; }
static void put32(uint8_t *s, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) s[i] = (uint8_t)(v >> (8 * i)); }
static bool same(const uint8_t *p, const void *q, size_t n)
{ const uint8_t *b = q; for (size_t i = 0; i < n; ++i) if (p[i] != b[i]) return false; return true; }
static bool zeros(const uint8_t *p, size_t n)
{ for (size_t i = 0; i < n; ++i) if (p[i]) return false; return true; }
static uint32_t crc32(const uint8_t *s, unsigned n)
{
    uint32_t c = UINT32_MAX;
    for (unsigned i = 0; i < n; ++i) {
        c ^= s[i];
        for (unsigned b = 0; b < 8; ++b) c = (c >> 1) ^ ((c & 1) ? UINT32_C(0xedb88320) : 0);
    }
    return ~c;
}

static bool state_valid(const struct upd_state *s)
{
    return s->generation && s->active_slot <= 2 && s->active_build && s->pending_slot <= 2 && s->tried <= 1 &&
           (s->pending_slot ? s->pending_slot != s->active_slot && s->pending_build > s->active_build :
                              !s->pending_build && !s->tried);
}

static bool state_read(struct upd_state *out, const uint8_t *s)
{
    if (!same(s, "STYUJR01", 8) || get32(s + 32) != crc32(s, 32) || !zeros(s + 36, UP_SECTOR - 36))
        return false;
    *out = (struct upd_state){get32(s + 8), get32(s + 12), get32(s + 16),
                              get32(s + 20), get32(s + 24), get32(s + 28)};
    return state_valid(out);
}

bool upd_open(struct upd_disk *d, const struct upd_io *io)
{
    d->valid = false;
    if (io->sectors != UP_DISK_SECTORS || !io->read || !io->write || !io->flush) return false;
    uint8_t sector[UP_SECTOR];
    if (!io->read(io->context, UP_REGION, sector) || !same(sector, "STYLEUP1", 8) ||
        get32(sector + 8) != 1 || get32(sector + 12) != UP_DISK_SECTORS ||
        get32(sector + 16) != UP_SLOT_A || get32(sector + 20) != UP_SLOT_B ||
        get32(sector + 24) != UP_SLOT_SECTORS || get32(sector + 28) != crc32(sector, 28) ||
        !zeros(sector + 32, UP_SECTOR - 32)) return false;
    struct upd_state a = {0}, b = {0};
    bool va = io->read(io->context, UP_JOURNAL_A, sector) && state_read(&a, sector);
    bool vb = io->read(io->context, UP_JOURNAL_B, sector) && state_read(&b, sector);
    if ((!va && !vb) || (va && vb && a.generation == b.generation)) return false;
    d->journal = vb && (!va || b.generation > a.generation);
    d->state = d->journal ? b : a;
    d->io = *io;
    d->valid = true;
    return true;
}

static bool record(struct upd_disk *d, struct upd_state s)
{
    if (!d->valid || d->state.generation == UINT32_MAX) return false;
    s.generation = d->state.generation + 1;
    if (!state_valid(&s)) return false;
    uint8_t out[UP_SECTOR] = "STYUJR01";
    put32(out + 8, s.generation);
    put32(out + 12, s.active_slot);
    put32(out + 16, s.active_build);
    put32(out + 20, s.pending_slot);
    put32(out + 24, s.pending_build);
    put32(out + 28, s.tried);
    put32(out + 32, crc32(out, 32));
    unsigned other = d->journal ^ 1;
    uint32_t lba = other ? UP_JOURNAL_B : UP_JOURNAL_A;
    uint8_t check[UP_SECTOR];
    if (!d->io.write(d->io.context, lba, out) || !d->io.flush(d->io.context) ||
        !d->io.read(d->io.context, lba, check) || !same(out, check, UP_SECTOR)) {
        d->valid = false;
        return false;
    }
    d->journal = other;
    d->state = s;
    return true;
}

static bool header(const uint8_t *p, struct upd_package *out)
{
    if (!same(p, "STYUPD01", 8) || get32(p + 8) != 1 || get32(p + 12) != 0x8664 ||
        !get32(p + 16) || get32(p + 20) < 128 || get32(p + 20) > UP_BLOB_MAX - UP_SECTOR ||
        !zeros(p + SIGNATURE_END, UP_SECTOR - SIGNATURE_END)) return false;
    *out = (struct upd_package){get32(p + 16), get32(p + 20)};
    return true;
}

enum upd_result upd_check(const uint8_t *p, size_t size, const uint8_t key[32], struct upd_package *out)
{
    struct upd_package rec;
    if (size < UP_SECTOR || !header(p, &rec) || size != (size_t)rec.bytes + UP_SECTOR) return UP_FORMAT;
    if (crypto_ed25519_check(p + SIGNED_BYTES, key, p, SIGNED_BYTES) != 0) return UP_AUTH;
    const uint8_t *efi = p + UP_SECTOR;
    uint8_t digest[64];
    crypto_sha512(digest, efi, rec.bytes);
    if (!same(digest, p + 24, sizeof(digest))) return UP_AUTH;
    uint32_t off = get32(efi + 60);
    if (!same(efi, "MZ", 2) || off < 64 || off > rec.bytes - 26 ||
        !same(efi + off, "PE\0\0\x64\x86", 6) || !same(efi + off + 24, "\x0b\x02", 2)) return UP_FORMAT;
    *out = rec;
    return UP_OK;
}

static uint32_t slot_start(unsigned slot) { return slot == 1 ? UP_SLOT_A : UP_SLOT_B; }

enum upd_result upd_load(struct upd_disk *d, unsigned slot, uint8_t buf[UP_BLOB_MAX],
                         const uint8_t key[32], struct upd_package *out)
{
    if (!d->valid || slot < 1 || slot > 2) return UP_DISK;
    struct upd_package p;
    uint32_t start = slot_start(slot);
    if (!d->io.read(d->io.context, start, buf)) return UP_IO;
    if (!header(buf, &p)) return UP_FORMAT;
    size_t size = UP_SECTOR + p.bytes;
    for (unsigned i = 1; i * UP_SECTOR < size; ++i)
        if (!d->io.read(d->io.context, start + i, buf + i * UP_SECTOR)) return UP_IO;
    return upd_check(buf, size, key, out);
}

enum upd_result upd_stage(struct upd_disk *d, uint8_t buf[UP_BLOB_MAX], size_t length, uint32_t running,
                          const uint8_t key[32], struct upd_package *out)
{
    if (!d->valid) return UP_DISK;
    if (d->state.pending_slot) return UP_BUSY;
    struct upd_package rec, readback;
    enum upd_result r = upd_check(buf, length, key, &rec);
    if (r != UP_OK) return r;
    if (rec.build <= running || rec.build <= d->state.active_build) return UP_OLD;
    unsigned slot = d->state.active_slot == 1 ? 2 : 1;
    uint32_t block = slot_start(slot);
    uint8_t last[UP_SECTOR] = {0};
    for (size_t off = 0; off < length; off += UP_SECTOR) {
        const uint8_t *s = buf + off;
        if (length - off < UP_SECTOR) {
            for (size_t i = 0; i < length - off; ++i) last[i] = s[i];
            s = last;
        }
        if (!d->io.write(d->io.context, block++, s)) return UP_IO;
    }
    if (!d->io.flush(d->io.context)) return UP_IO;
    r = upd_load(d, slot, buf, key, &readback);
    if (r != UP_OK) return r;
    if (readback.build != rec.build || readback.bytes != rec.bytes) return UP_AUTH;
    struct upd_state next = d->state;
    next.pending_slot = slot;
    next.pending_build = rec.build;
    if (!record(d, next)) return UP_IO;
    *out = rec;
    return UP_OK;
}

bool upd_trial(struct upd_disk *d)
{
    if (!d->valid || !d->state.pending_slot || d->state.tried) return false;
    struct upd_state next = d->state;
    next.tried = 1;
    return record(d, next);
}

bool upd_rollback(struct upd_disk *d)
{
    if (!d->valid || !d->state.pending_slot) return false;
    struct upd_state next = d->state;
    next.pending_slot = next.pending_build = next.tried = 0;
    return record(d, next);
}

bool upd_accept(struct upd_disk *d, unsigned slot, uint32_t build)
{
    if (!d->valid || !slot || !d->state.tried || slot != d->state.pending_slot || build != d->state.pending_build)
        return false;
    struct upd_state next = d->state;
    next.active_slot = slot;
    next.active_build = build;
    next.pending_slot = next.pending_build = next.tried = 0;
    return record(d, next);
}
