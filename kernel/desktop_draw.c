#include <styleos/desktop_v0.h>
#include "desktop_font.inc"

static const struct desktop_surface *fb;
static int width, height;

void desk_draw_init(const struct desktop_surface *surface)
{
    fb = surface;
    width = (int)surface->width;
    height = (int)surface->height;
}

static uint32_t native(uint32_t c)
{
    if (fb->format == DESKTOP_BGR_BYTES) return c;
    return (c & 0xff00u) | ((c & 0xffu) << 16) | ((c >> 16) & 0xffu);
}

void desk_fill(int x, int y, int w, int h, uint32_t rgb)
{
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > width) x1 = width;
    if (y1 > height) y1 = height;
    if (x >= x1 || y >= y1) return;
    uint32_t color = native(rgb);
    volatile uint32_t *line = (volatile uint32_t *)fb->base + (size_t)y * fb->stride + x;
    for (int row = y; row < y1; ++row, line += fb->stride)
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
    for (int row = 0; row < height; ++row) {
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
    uint32_t color = native(rgb);
    for (int row = 0; row < 20; ++row) {
        int yy = y + row;
        if (yy < 0 || yy >= height) continue;
        uint32_t mask = f->rows[row];
        volatile uint32_t *line = (volatile uint32_t *)fb->base + (size_t)yy * fb->stride;
        for (int col = 0; mask && x + col < right; ++col, mask >>= 2) {
            int xx = x + col;
            unsigned alpha = mask & 3;
            if (alpha && xx >= 0 && xx < width)
                line[xx] = alpha == 3 ? color : mix(line[xx], color, alpha, 3);
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
