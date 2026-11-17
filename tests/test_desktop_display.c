#include <assert.h>
#include <styleos/boot_memory.h>
#include <styleos/desktop_display.h>

static uint32_t pixels[323u * 240u + 2u];
static struct boot_memory_map map;

int main(void)
{
    struct desktop_surface s = {
        (uintptr_t)&pixels[1], (sizeof(pixels) - 2u * sizeof(*pixels)),
        320, 240, 323, DESKTOP_BGR_BYTES
    };
    struct boot_memory_evidence e = {.map = &map};
    assert(desktop_surface_valid(&s));
    assert(desktop_surface_excludes_boot_memory(&s, &e));
    e.live.image = (struct boot_range){s.base - 4u, s.base + 4u};
    assert(!desktop_surface_excludes_boot_memory(&s, &e));
    e.live.image.end = s.base;
    assert(desktop_surface_excludes_boot_memory(&s, &e));
    map.nr_ram = 1;
    map.ram[0] = (struct boot_range){s.base + s.bytes - 4u, s.base + s.bytes + 4u};
    assert(!desktop_surface_excludes_boot_memory(&s, &e));
    for (size_t i = 0; i < sizeof(pixels) / sizeof(*pixels); ++i)
        pixels[i] = 0xa55aa55au;
    struct desktop_surface invalid = s;
    invalid.bytes--;
    assert(!desktop_draw_check(&invalid) && pixels[1] == 0xa55aa55au);
    invalid = s; invalid.stride = 319;
    assert(!desktop_surface_valid(&invalid));
    invalid = s; invalid.base = UINTPTR_MAX - 3;
    assert(!desktop_surface_valid(&invalid));
    invalid = s; invalid.format = (enum desktop_pixel_format)3;
    assert(!desktop_surface_valid(&invalid));
    assert(desktop_draw_check(&s));
    assert(pixels[0] == 0xa55aa55au && pixels[1] == 0x171c24u);
    assert(pixels[sizeof(pixels) / sizeof(*pixels) - 1] == 0xa55aa55au);
    for (size_t row = 0; row < s.height; ++row)
        for (size_t col = s.width; col < s.stride; ++col)
            assert(pixels[1 + row * s.stride + col] == 0xa55aa55au);
    assert(pixels[1 + 239u * s.stride + 319u] == 0x202731u);
    s.format = DESKTOP_RGB_BYTES;
    assert(desktop_draw_check(&s));
    assert(pixels[1] == 0x241c17u);
    return 0;
}
