#include <styleos/desktop_v0.h>

#include <assert.h>
#include <stdio.h>

enum { W = 56, H = 72, STRIDE = 64, UNTOUCHED = 0x01234567 };
static uint32_t pixels[H][STRIDE];
static struct desk_box guard;
static uint32_t guard_before;
static unsigned rendered;

static void paint(void)
{
    ++rendered;
    desk_fill(-5, -5, W + 10, H + 10, 0x123456);
    desk_fill(11, 14, 1, 1, 0xaabbcc);
    /* Clearing the region and then drawing the foreground must not expose
     * the temporary background (or the foreground) until the copy. */
    if (desk_clip_visible(guard)) assert(pixels[guard.y][guard.x] == guard_before);
}

static bool inside(struct desk_box b, int x, int y)
{
    return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

int main(void)
{
    const struct desktop_surface surface = {(uintptr_t)pixels, sizeof(pixels), W, H, STRIDE,
                                            DESKTOP_BGR_BYTES};
    desk_draw_init(&surface);
    for (unsigned y = 0; y < H; ++y)
        for (unsigned x = 0; x < STRIDE; ++x) pixels[y][x] = UNTOUCHED;

    struct desk_damage none = {0};
    assert(desk_present(&none, paint) == 0 && rendered == 0);
    puts("PASS idle frame writes no pixels");

    struct desk_damage move = {0};
    struct desk_box old = {6, 12, 10, 17}, next = {40, 44, 10, 17};
    desk_damage_add(&move, old);
    desk_damage_add(&move, next);
    assert(move.count == 2 && !move.all);
    guard = (struct desk_box){11, 14, 1, 1};
    guard_before = UNTOUCHED;
    assert(desk_present(&move, paint) == 340 && rendered == 2);
    for (int y = 0; y < H; ++y) for (int x = 0; x < STRIDE; ++x) {
        uint32_t color = inside(old, x, y) || inside(next, x, y) ? 0x123456 : UNTOUCHED;
        if (inside(guard, x, y)) color = 0xaabbcc;
        assert(pixels[y][x] == color);
    }
    puts("PASS two cursor rectangles, completed before exposure, no unrelated writes");

    struct desk_damage edge = {0};
    desk_damage_add(&edge, (struct desk_box){W - 3, H - 5, 10, 17});
    assert(desk_present(&edge, paint) == 15);
    assert(pixels[H-1][W-1] == 0x123456 && pixels[H-1][W] == UNTOUCHED);
    puts("PASS cursor clipping preserves scanline padding");

    struct desk_damage all = {.all = true};
    guard = (struct desk_box){1, H - 1, 1, 1};
    guard_before = UNTOUCHED;
    rendered = 0;
    assert(desk_present(&all, paint) == W * H && rendered >= 2);
    assert(pixels[H-1][1] == 0x123456 && pixels[H-1][W] == UNTOUCHED);
    puts("PASS full scene presented in complete clipped strips");
}
