#include <styleos/desktop_v0.h>
#include "desktop_font.inc"

static const struct desktop_surface *fb;
static int width, height;
/* One offscreen strip instead of allocating a second full-size framebuffer.
 * Even a small cursor box is painted completely before GOP writes begin. */
enum { DESK_MAX_WIDTH = 4096, STRIP_HEIGHT = 32 };
static uint32_t strip[DESK_MAX_WIDTH * STRIP_HEIGHT];
static struct desk_box clip;

static int lo(int a, int b) { return a < b ? a : b; }
static int hi(int a, int b) { return a > b ? a : b; }
static bool intersects(struct desk_box a, struct desk_box b)
{
    return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 &&
           a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
bool desk_box_equal(struct desk_box a, struct desk_box b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
bool desk_clip_visible(struct desk_box b) { return intersects(clip, b); }

void desk_damage_add(struct desk_damage *d, struct desk_box b)
{
    if (d->all || b.w <= 0 || b.h <= 0) return;
    for (unsigned i = 0; i < d->count;) {
        struct desk_box a = d->boxes[i];
        if (!intersects(a, b)) { ++i; continue; }
        int x = lo(a.x, b.x), y = lo(a.y, b.y);
        struct desk_box u = {x, y, hi(a.x + a.w, b.x + b.w) - x, hi(a.y + a.h, b.y + b.h) - y};
        /* Do not turn a diagonal or distant pointer move into a large update. */
        if (u.w * u.h > a.w * a.h + b.w * b.h) { ++i; continue; }
        b = u;
        d->boxes[i] = d->boxes[--d->count];
        i = 0;
    }
    if (d->count == DESK_DAMAGE_CAPACITY) d->all = true;
    else d->boxes[d->count++] = b;
}

void desk_draw_init(const struct desktop_surface *surface)
{
    fb = surface;
    width = (int)surface->width;
    height = (int)surface->height;
}

size_t desk_present(const struct desk_damage *damage, void (*draw_scene)(void))
{
    size_t writes = 0;
    if (width > DESK_MAX_WIDTH) return 0;
    for (unsigned at = 0; at < (damage->all ? 1u : damage->count); ++at) {
        struct desk_box b = damage->all ? (struct desk_box){0, 0, width, height} : damage->boxes[at];
        int x = hi(0, b.x), end = lo(width, b.x + b.w);
        if (x >= end) continue;
        int bottom = lo(height, b.y + b.h);
        for (int y = hi(0, b.y); y < bottom; y += STRIP_HEIGHT) {
            clip = (struct desk_box){x, y, end - x, lo(STRIP_HEIGHT, bottom - y)};
            draw_scene();
            for (int row = 0; row < clip.h; ++row) {
                volatile uint32_t *to = (volatile uint32_t *)fb->base + (size_t)(y + row) * fb->stride + x;
                const uint32_t *from = strip + (size_t)row * clip.w;
                for (int col = 0; col < clip.w; ++col) to[col] = from[col];
            }
            writes += (size_t)clip.w * clip.h;
        }
    }
    clip = (struct desk_box){0};
    return writes;
}

static uint32_t native(uint32_t c)
{
    if (fb->format == DESKTOP_BGR_BYTES) return c;
    return (c & 0xff00u) | ((c & 0xffu) << 16) | ((c >> 16) & 0xffu);
}

void desk_fill(int x, int y, int w, int h, uint32_t rgb)
{
    int x1 = x + w, y1 = y + h;
    x = hi(x, clip.x);
    y = hi(y, clip.y);
    x1 = lo(x1, clip.x + clip.w);
    y1 = lo(y1, clip.y + clip.h);
    if (x >= x1 || y >= y1) return;
    uint32_t color = native(rgb);
    uint32_t *line = strip + (size_t)(y - clip.y) * clip.w + (x - clip.x);
    for (int row = y; row < y1; ++row, line += clip.w)
        for (int col = 0; col < x1 - x; ++col) line[col] = color;
}

static uint32_t mix(uint32_t a, uint32_t b, unsigned t, unsigned max)
{
    unsigned r = (((a >> 16) & 255) * (max - t) + ((b >> 16) & 255) * t) / max;
    unsigned g = (((a >> 8) & 255) * (max - t) + ((b >> 8) & 255) * t) / max;
    unsigned v = ((a & 255) * (max - t) + (b & 255) * t) / max;
    return r << 16 | g << 8 | v;
}

void desk_wallpaper(void)
{
    /* Navy background and two low angular folds from the reference. */
    for (int row = clip.y; row < clip.y + clip.h; ++row) {
        int t = row * 864 / height;
        uint32_t ground = t < 480 ? mix(0x192332, 0x1d2a3e, (unsigned)t, 480) :
                         mix(0x1d2a3e, 0x162031, (unsigned)(t - 480), 384);
        desk_fill(0, row, width, 1, ground);
        int rising = 1536 - (t - 614) * 5;
        if (rising < 1536) {
            int x = rising > 0 ? rising * width / 1536 : 0;
            desk_fill(x, row, width - x, 1, 0x29364a);
        }
        int left = 933 - (t - 714) * 7;
        int right = 933 + (t - 714) * 13;
        if (t >= 714) desk_fill(left * width / 1536, row,
                               (right - left) * width / 1536, 1, 0x223047);
    }
}

void desk_round(struct desk_box b, uint32_t rgb)
{
    if (b.w < 10 || b.h < 10) { desk_fill(b.x, b.y, b.w, b.h, rgb); return; }
    desk_fill(b.x + 4, b.y, b.w - 8, 1, rgb);
    desk_fill(b.x + 2, b.y + 1, b.w - 4, 2, rgb);
    desk_fill(b.x, b.y + 3, b.w, b.h - 6, rgb);
    desk_fill(b.x + 2, b.y + b.h - 3, b.w - 4, 2, rgb);
    desk_fill(b.x + 4, b.y + b.h - 1, b.w - 8, 1, rgb);
}

void desk_outline(struct desk_box b, uint32_t rgb)
{
    desk_fill(b.x + 4, b.y, b.w - 8, 1, rgb);
    desk_fill(b.x + 4, b.y + b.h - 1, b.w - 8, 1, rgb);
    desk_fill(b.x, b.y + 3, 1, b.h - 6, rgb);
    desk_fill(b.x + b.w - 1, b.y + 3, 1, b.h - 6, rgb);
    desk_fill(b.x + 1, b.y + 1, 3, 1, rgb);
    desk_fill(b.x + b.w - 4, b.y + 1, 3, 1, rgb);
    desk_fill(b.x + 1, b.y + b.h - 2, 3, 1, rgb);
    desk_fill(b.x + b.w - 4, b.y + b.h - 2, 3, 1, rgb);
}

static int positive(int n) { return n < 0 ? -n : n; }
void desk_line(int x, int y, int ex, int ey, uint32_t rgb)
{
    int dx = positive(ex - x), dy = -positive(ey - y);
    int sx = x < ex ? 1 : -1, sy = y < ey ? 1 : -1, e = dx + dy;
    for (;;) {
        desk_fill(x, y, 1, 1, rgb);
        if (x == ex && y == ey) break;
        int p = e * 2;
        if (p >= dy) { e += dy; x += sx; }
        if (p <= dx) { e += dx; y += sy; }
    }
}

uint32_t desk_utf8(const char *s, size_t n, size_t *used)
{
    *used = 0;
    if (!n) return 0;
    uint8_t a = (uint8_t)s[0];
    *used = 1;
    if (a < 0x80) return a;
    unsigned bytes = a < 0xc2 ? 0 : a < 0xe0 ? 2 : a < 0xf0 ? 3 : a < 0xf5 ? 4 : 0;
    if (!bytes || n < bytes) return 0xfffd;
    uint32_t cp = a & ((1u << (7 - bytes)) - 1);
    for (unsigned i = 1; i < bytes; ++i) {
        uint8_t b = (uint8_t)s[i];
        if ((b & 0xc0) != 0x80) return 0xfffd;
        cp = cp << 6 | (b & 0x3f);
    }
    if ((bytes == 3 && cp < 0x800) || (bytes == 4 && cp < 0x10000) ||
        (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff) return 0xfffd;
    *used = bytes;
    return cp;
}

int desk_glyph(int x, int y, int right, uint32_t cp, uint32_t rgb)
{
    unsigned g = cp >= 32 && cp < 127 ? cp - 32 :
                 cp == 0x401 ? 95 : cp >= 0x410 && cp <= 0x44f ? 96 + cp - 0x410 :
                 cp == 0x451 ? 160 : '?' - 32;
    const struct desk_font_glyph *f = &desk_font[g];
    if (!desk_clip_visible((struct desk_box){x, y, 16, 20})) return f->step;
    uint32_t color = native(rgb);
    for (int row = 0; row < 20; ++row) {
        int yy = y + row;
        if (yy < clip.y || yy >= clip.y + clip.h) continue;
        uint32_t mask = f->rows[row];
        uint32_t *line = strip + (size_t)(yy - clip.y) * clip.w;
        for (int col = 0; mask && x + col < right; ++col, mask >>= 2) {
            int xx = x + col;
            unsigned alpha = mask & 3;
            if (alpha && xx >= clip.x && xx < clip.x + clip.w) {
                int i = xx - clip.x;
                line[i] = alpha == 3 ? color : mix(line[i], color, alpha, 3);
            }
        }
    }
    return f->step;
}

int desk_text(int x, int y, int right, const char *label, uint32_t rgb)
{
    size_t len = 0;
    while (label[len]) ++len;
    for (size_t off = 0; off < len && x < right;) {
        size_t n;
        uint32_t cp = desk_utf8(label + off, len - off, &n);
        x += desk_glyph(x, y, right, cp, rgb);
        off += n;
    }
    return x;
}

void desk_icon(int x, int y, int size, unsigned icon, uint32_t rgb)
{
    int b = size, r = b - 2;
    switch (icon) {
    case DESK_FILE_MANAGER: {
        /* App shortcut: a split-pane window, never a standalone folder.
         * Keep the silhouette legible at 23 px on the taskbar as well. */
        int top = y + b / 8, h = b * 3 / 4, t = hi(1, b / 18);
        desk_round((struct desk_box){x, top, b, h}, 0xa2bbd4);
        desk_round((struct desk_box){x + t, top + t, b - 2*t, h - 2*t}, 0x1c2b3e);
        desk_fill(x + 2*t, top + 2*t, b - 4*t, b / 6, 0x496680);
        int row = top + b / 3;
        desk_fill(x + b / 7, row, b / 4, h - b / 3 - 2*t, 0x6d88a7);
        for (int line = 0; line < 2; ++line) {
            int yy = row + line * b / 6;
            desk_fill(x + b / 2, yy, b / 9, b / 9, 0xe7be80);
            desk_fill(x + 2*b / 3, yy + t, b / 5, t, rgb);
        }
        break;
    }
    case DESK_FOLDER:
        desk_round((struct desk_box){x, y + b / 7, b * 5 / 8, b / 2}, rgb);
        desk_round((struct desk_box){x, y + b / 4, b, b * 2 / 3}, rgb);
        desk_fill(x + 2, y + b / 4 + 1, b - 4, 1, mix(rgb, 0xffffff, 1, 4));
        break;
    case DESK_PAPER:
        x += b / 8; y += b / 12; b -= b / 4;
        desk_round((struct desk_box){x, y, b, b + b / 8}, rgb);
        for (int i = 0; i < 3; ++i)
            desk_fill(x + b / 5, y + b / 4 + i * b / 5,
                      b * (i == 2 ? 2 : 3) / 5, 2, 0x283347);
        break;
    case DESK_LOGO:
        desk_fill(x, y + b / 2, b * 3 / 4, b / 3, rgb);
        desk_fill(x + b / 3, y + b / 6, b * 2 / 3, b / 3, rgb);
        break;
    case DESK_TASK:
        desk_fill(x + 2, y + b / 2, b / 6, b / 2, rgb);
        desk_fill(x + b / 3, y + b / 3, b / 6, b * 2 / 3, rgb);
        desk_fill(x + b * 2 / 3, y + 2, b / 6, b - 2, rgb);
        break;
    default:
        for (int row = 0; row < b; ++row) for (int col = 0; col < b; ++col) {
            int dx = col * 2 + 1 - b, dy = row * 2 + 1 - b;
            int d = dx * dx + dy * dy;
            if (d <= b * b && d >= r * r &&
                !((icon == DESK_POWER && row < b / 5) ||
                  (icon == DESK_RESTART && col >= b * 3 / 4 && row < b / 4)))
                desk_fill(x + col, y + row, 1, 1, rgb);
        }
        if (icon == DESK_INFO) {
            desk_fill(x + b / 2 - 1, y + b / 4, 2, 2, rgb);
            desk_fill(x + b / 2 - 1, y + b / 2, 2, b / 4, rgb);
        } else if (icon == DESK_POWER) desk_fill(x + b / 2 - 1, y, 2, b / 2, rgb);
        else {
            desk_line(x + b - 2, y + 1, x + b - 2, y + b / 3, rgb);
            desk_line(x + b - 2, y + b / 3, x + b * 2 / 3, y + b / 3, rgb);
        }
        break;
    }
}
