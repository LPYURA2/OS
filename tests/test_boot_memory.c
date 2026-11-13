#include "styleos/boot_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect(struct boot_range r, uint64_t a, uint64_t b)
{
    assert(r.start == a && r.end == b);
}

static void normalized(void)
{
    struct boot_memory_map m = {0};
    assert(!bm_add(&m, 0xb000, 0x3000, 0));
    assert(!bm_add(&m, 0x1001, 0x9fff, 0));
    assert(!bm_add(&m, 0x4fff, 2, 1));
    assert(!bm_add(&m, 0x5200, 0x1e00, 1));
    assert(!bm_add(&m, 0xa800, 0x1000, 1));
    assert(!bm_normalize(&m));
    assert(m.nr_candidates == 3 && m.nr_reserved == 2);
    expect(m.candidates[0], 0x2000, 0x4000);
    expect(m.candidates[1], 0x7000, 0xa000);
    expect(m.candidates[2], 0xc000, 0xe000);
    expect(m.reserved[0], 0x4000, 0x7000);
    expect(m.reserved[1], 0xa000, 0xc000);

    struct boot_memory_map reverse = {0};
    assert(!bm_add(&reverse, 0x1001, 0x9fff, 0));
    assert(!bm_add(&reverse, 0xb000, 0x3000, 0));
    assert(!bm_add(&reverse, 0xa800, 0x1000, 1));
    assert(!bm_add(&reverse, 0x5200, 0x1e00, 1));
    assert(!bm_add(&reverse, 0x4fff, 2, 1));
    assert(!bm_normalize(&reverse));
    assert(!memcmp(m.candidates, reverse.candidates,
                   m.nr_candidates * sizeof(struct boot_range)));

    m = (struct boot_memory_map){0};
    assert(!bm_add(&m, 0x1000, 0x1000, 0));
    assert(!bm_add(&m, 0x2000, 0x1000, 0));
    assert(!bm_normalize(&m));
    assert(m.nr_candidates == 1);
    expect(m.candidates[0], 0x1000, 0x3000);
}

static void invalid_ranges(void)
{
    struct boot_memory_map m = {0};
    assert(bm_normalize(&m) == BM_EMPTY && !m.nr_candidates);
    assert(bm_add(&m, 1, 0, 1) == BM_RANGE);
    assert(bm_add(&m, UINT64_MAX - 3, 4, 1) == BM_OVERFLOW);
    assert(!bm_add(&m, UINT64_MAX - 5, 3, 1));
    assert(bm_normalize(&m) == BM_OVERFLOW && !m.nr_candidates);
    m = (struct boot_memory_map){0};
    assert(!bm_add(&m, 0x1000, 0x2000, 0));
    assert(!bm_add(&m, 0x2000, 0x1000, 0));
    assert(bm_normalize(&m) == BM_CONFLICT && !m.nr_candidates);
    m = (struct boot_memory_map){0};
    assert(!bm_add(&m, 0x1000, 0x1000, 0));
    assert(!bm_add(&m, 0x1000, 0x1000, 1));
    assert(bm_normalize(&m) == BM_EMPTY && !m.nr_candidates);
    m = (struct boot_memory_map){0};
    for (size_t i = 0; i < BOOT_MEMORY_INPUT_MAX; ++i)
        assert(!bm_add(&m, (i * 2 + 1) * 4096, 4096, 1));
    assert(bm_add(&m, 0, 1, 1) == BM_LIMIT);
    assert(!bm_add(&m, 0, (BOOT_MEMORY_INPUT_MAX * 2 + 1) * 4096, 0));
    assert(bm_normalize(&m) == BM_LIMIT && !m.nr_candidates);
}

static void le(uint8_t *p, unsigned n, uint64_t value)
{
    for (unsigned i = 0; i < n; ++i) { p[i] = value & 255; value >>= 8; }
}

static void descriptor(uint8_t *p, unsigned type, uint64_t base,
                       uint64_t pages, uint64_t attr)
{
    le(p, 4, type); le(p + 8, 8, base); le(p + 24, 8, pages); le(p + 32, 8, attr);
}

static void uefi(void)
{
    struct boot_memory_map m = {0};
    uint8_t map[6 * 48];
    memset(map, 0xa5, sizeof map);
    descriptor(map, 7, 0xa000, 2, 0xf);
    descriptor(map + 48, 0xffffffffu, 0x9000, 1, 8);
    descriptor(map + 96, 7, 0x3000, 1, UINT64_C(0x8000000000000008));
    descriptor(map + 144, 7, 0x1000, 1, 0x20008);
    descriptor(map + 192, 7, 0x2000, 1, 0x1);
    descriptor(map + 240, 2, 0x6000, 1, 8);
    assert(bm_uefi_parse(&m, map, sizeof map, 40, 1) == BM_UEFI_FORMAT);
    assert(bm_uefi_parse(&m, map, sizeof map - 1, 48, 1) == BM_UEFI_FORMAT);
    assert(bm_uefi_parse(&m, map, sizeof map, 48, 2) == BM_UEFI_FORMAT);
    assert(!bm_uefi_parse(&m, map, sizeof map, 48, 1));
    assert(m.nr_ram == 1 && m.nr_reserved == 5);
    assert(!bm_normalize(&m));
    expect(m.candidates[0], 0xa000, 0xc000);
    assert(m.nr_candidates == 1);
    assert(map[40] == 0xa5 && map[88] == 0xa5);
    m = (struct boot_memory_map){0};
    descriptor(map + 48, 11, 0xa000, 1, 8);
    assert(bm_uefi_parse(&m, map, sizeof map, 48, 1) == BM_CONFLICT);
    m = (struct boot_memory_map){0};
    descriptor(map, 7, 0x1000, UINT64_MAX, 8);
    assert(bm_uefi_parse(&m, map, sizeof map, 48, 1) == BM_OVERFLOW);
    descriptor(map, 7, UINT64_MAX & ~UINT64_C(4095), 1, 8);
    assert(bm_uefi_parse(&m, map, sizeof map, 48, 1) == BM_OVERFLOW);
    descriptor(map, 7, 0x1001, 1, 8);
    assert(bm_uefi_parse(&m, map, sizeof map, 48, 1) == BM_UEFI_FORMAT);
}

static void be(uint8_t *p, uint32_t value)
{
    for (int i = 3; i >= 0; --i) { p[i] = value & 255; value >>= 8; }
}

struct fdt { uint8_t buf[1024]; size_t p; };
static void word(struct fdt *f, uint32_t v) { be(f->buf + f->p, v); f->p += 4; }
static void begin(struct fdt *f, const char *name)
{
    word(f, 1);
    size_t n = strlen(name) + 1;
    memcpy(f->buf + f->p, name, n);
    f->p += (n + 3) & ~(size_t)3;
}
static const char strings[] =
    "#address-cells\0#size-cells\0device_type\0reg\0ranges\0size\0no-map\0reusable\0";
enum { AC = 0, SC = 15, TYPE = 27, REG = 39, RANGES = 43,
       SIZE = 50, NO_MAP = 55, REUSABLE = 62 };
static void prop(struct fdt *f, uint32_t id, uint32_t len)
{
    word(f, 3); word(f, len); word(f, id);
}
static void bus(struct fdt *f, uint32_t n)
{
    prop(f, AC, 4); word(f, n);
    prop(f, SC, 4); word(f, n);
}
static void reg(struct fdt *f, unsigned n, uint32_t addr, uint32_t len)
{
    prop(f, REG, n * 8);
    if (n == 2) word(f, 0);
    word(f, addr);
    if (n == 2) word(f, 0);
    word(f, len);
}
static struct fdt fixture(unsigned n, int dynamic)
{
    struct fdt f = {.p = 72};
    begin(&f, ""); bus(&f, n);
    begin(&f, "memory@1000"); prop(&f, TYPE, 7);
    memcpy(f.buf + f.p, "memory", 7); f.p += 8;
    reg(&f, n, 0x1000, 0xf000); word(&f, 2);
    begin(&f, "reserved-memory"); bus(&f, n); prop(&f, RANGES, 0);
    begin(&f, "protected@3800");
    if (dynamic) { prop(&f, SIZE, 4); word(&f, 0x2000); }
    else reg(&f, n, 0x3800, 0x1800);
    prop(&f, NO_MAP, 0); word(&f, 2);
    begin(&f, "temporary@8000"); reg(&f, n, 0x8000, 0x1000);
    prop(&f, REUSABLE, 0); word(&f, 2);
    word(&f, 2); word(&f, 2); word(&f, 9);
    size_t strings_at = f.p;
    memcpy(f.buf + f.p, strings, sizeof strings);
    f.p += sizeof strings;
    be(f.buf, 0xd00dfeed); be(f.buf + 4, f.p);
    be(f.buf + 8, 72); be(f.buf + 12, strings_at); be(f.buf + 16, 40);
    be(f.buf + 20, 17); be(f.buf + 24, 16);
    be(f.buf + 32, sizeof strings); be(f.buf + 36, strings_at - 72);
    be(f.buf + 44, 0x2800); be(f.buf + 52, 0x1000);
    return f;
}

static void device_tree(void)
{
    struct boot_memory_map m = {0};
    uint32_t total = 0;
    for (unsigned n = 1; n <= 2; ++n) {
        struct fdt f = fixture(n, 0);
        assert(!bm_fdt_parse(&m, f.buf, f.p, &total) && total == f.p);
        assert(m.nr_reserved == 3 && m.nr_ram == 1);
        assert(!bm_normalize(&m) && m.nr_candidates == 3);
        expect(m.candidates[0], 0x1000, 0x2000);
        expect(m.candidates[1], 0x5000, 0x8000);
        expect(m.candidates[2], 0x9000, 0x10000);
        m = (struct boot_memory_map){0};
    }
    struct fdt f = fixture(2, 0);
    for (size_t n = 0; n < f.p; ++n)
        assert(bm_fdt_parse(&m, f.buf, n, &total) == BM_FDT_FORMAT);
    be(f.buf + 60, 0x3000); be(f.buf + 68, 0x1000);
    assert(bm_fdt_parse(&m, f.buf, f.p, &total) == BM_CONFLICT);
    m = (struct boot_memory_map){0};
    be(f.buf + 60, 0x6000);
    assert(bm_fdt_parse(&m, f.buf, f.p, &total) == BM_FDT_FORMAT);
    m = (struct boot_memory_map){0};
    f = fixture(2, 1);
    assert(bm_fdt_parse(&m, f.buf, f.p, &total) == BM_FDT_UNSUPPORTED);
    m = (struct boot_memory_map){0};
    f = fixture(2, 0); be(f.buf + 92, 3);
    assert(bm_fdt_parse(&m, f.buf, f.p, &total) == BM_FDT_UNSUPPORTED);
    m = (struct boot_memory_map){0};
    f = fixture(2, 0); be(f.buf + 88, UINT32_MAX);
    assert(bm_fdt_parse(&m, f.buf, f.p, &total) == BM_FDT_FORMAT);
    m = (struct boot_memory_map){0};
    f = fixture(2, 0); be(f.buf + 8, 56);
    assert(bm_fdt_parse(&m, f.buf, f.p, &total) == BM_FDT_FORMAT);
}

int main(void)
{
    normalized();
    invalid_ranges();
    uefi();
    device_tree();
    puts("Boot memory: normalization, limits, UEFI and FDT PASS");
    return 0;
}
