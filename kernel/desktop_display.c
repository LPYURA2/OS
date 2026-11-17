#include <styleos/desktop_display.h>
#include <styleos/boot_memory.h>

enum {
    DISPLAY_MAX_EDGE = 4096,
    DISPLAY_MIN_WIDTH = 320,
    DISPLAY_MIN_HEIGHT = 240,
};

bool desktop_surface_valid(const struct desktop_surface *s)
{
    if (!s || !s->base || (s->base & 3u) ||
        s->width < DISPLAY_MIN_WIDTH || s->width > DISPLAY_MAX_EDGE ||
        s->height < DISPLAY_MIN_HEIGHT || s->height > DISPLAY_MAX_EDGE ||
        s->stride < s->width || s->stride > DISPLAY_MAX_EDGE ||
        (s->format != DESKTOP_RGB_BYTES && s->format != DESKTOP_BGR_BYTES))
        return false;
    uint64_t span = UINT64_C(4) * s->stride * s->height;
    return span <= s->bytes && span <= UINTPTR_MAX - s->base;
}

static bool overlaps(struct boot_range a, struct boot_range b)
{
    return a.start < b.end && b.start < a.end;
}

bool desktop_surface_excludes_boot_memory(const struct desktop_surface *s,
                                          const struct boot_memory_evidence *e)
{
    if (!desktop_surface_valid(s) || !e || !e->map ||
        e->map->nr_ram > BOOT_MEMORY_INPUT_MAX)
        return false;
    const struct boot_range fb = {
        s->base, s->base + UINT64_C(4) * s->stride * s->height
    };
    if (overlaps(fb, e->live.image) || overlaps(fb, e->live.stack) ||
        overlaps(fb, e->live.handoff) || overlaps(fb, e->live.source) ||
        overlaps(fb, e->workspace))
        return false;
    for (size_t i = 0; i < e->map->nr_ram; ++i)
        if (overlaps(fb, e->map->ram[i]))
            return false;
    return true;
}

static uint32_t pixel(const struct desktop_surface *s, uint32_t rgb)
{
    if (s->format == DESKTOP_BGR_BYTES)
        return rgb;
    return ((rgb & 0xffu) << 16) | (rgb & 0xff00u) | ((rgb >> 16) & 0xffu);
}

static void rect(const struct desktop_surface *s, uint32_t x, uint32_t y,
                 uint32_t w, uint32_t h, uint32_t rgb)
{
    if (x >= s->width || y >= s->height || !w || !h)
        return;
    if (w > s->width - x)
        w = s->width - x;
    if (h > s->height - y)
        h = s->height - y;
    volatile uint32_t *base = (volatile uint32_t *)s->base;
    uint32_t color = pixel(s, rgb);
    for (uint32_t row = y; row < y + h; ++row) {
        volatile uint32_t *line = base + (uint64_t)row * s->stride + x;
        for (uint32_t column = 0; column < w; ++column)
            line[column] = color;
    }
}

/* Seven five-bit rows per glyph. Restricted to the two diagnostic labels. */
static const struct {
    char letter;
    uint8_t rows[7];
} glyphs[] = {
    {'A',{14,17,17,31,17,17,17}}, {'C',{14,17,16,16,16,17,14}},
    {'D',{30,17,17,17,17,17,30}}, {'E',{31,16,16,30,16,16,31}},
    {'H',{17,17,17,31,17,17,17}}, {'I',{31,4,4,4,4,4,31}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
    {'Y',{17,17,10,4,4,4,4}},
};

static void text(const struct desktop_surface *s, uint32_t x, uint32_t y,
                 uint32_t unit, const char *label, uint32_t color)
{
    for (; *label && x < s->width; ++label, x += 6u * unit) {
        for (unsigned g = 0; g < sizeof(glyphs) / sizeof(glyphs[0]); ++g) {
            if (glyphs[g].letter != *label)
                continue;
            for (unsigned row = 0; row < 7; ++row)
                for (unsigned col = 0; col < 5; ++col)
                    if (glyphs[g].rows[row] & (16u >> col))
                        rect(s, x + col * unit, y + row * unit, unit, unit, color);
            break;
        }
    }
}

bool desktop_draw_check(const struct desktop_surface *s)
{
    if (!desktop_surface_valid(s))
        return false;
    uint32_t unit = s->width >= 640 ? 5 : 3;
    uint32_t x = (s->width - 41u * unit) / 2u;
    uint32_t y = s->height / 2u - 7u * unit;
    rect(s, 0, 0, s->width, s->height, 0x171c24);
    rect(s, 0, s->height - 36u, s->width, 36, 0x202731);
    rect(s, 0, s->height - 37u, s->width, 1, 0x343d48);
    rect(s, x, y - 5u * unit, 9u * unit, unit, 0xe9ab58);
    text(s, x, y, unit, "STYLEOS", 0xf1efe9);
    text(s, 16, s->height - 25u, 2, "DISPLAY CHECK", 0xa6aeb8);
    return true;
}
