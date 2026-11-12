#include "mw_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MW_Engine { int width; int height; uint8_t *rgb; };
MW_Engine *mw_create(int width, int height) {
    if (width < 1 || height < 1 || width > 4096 || height > 4096) return NULL;
    MW_Engine *e = (MW_Engine *)calloc(1, sizeof(MW_Engine));
    if (!e) return NULL;
    e->rgb = (uint8_t *)calloc((size_t)width * (size_t)height, 3);
    if (!e->rgb) { free(e); return NULL; }
    e->width = width; e->height = height;
    return e;
}
void mw_destroy(MW_Engine *e) { if (e) { free(e->rgb); free(e); } }
int mw_width(const MW_Engine *e) { return e ? e->width : 0; }
int mw_height(const MW_Engine *e) { return e ? e->height : 0; }
void mw_pixel(MW_Engine *e, int x, int y, uint32_t color) {
    if (!e || x < 0 || y < 0 || x >= e->width || y >= e->height) return;
    size_t idx = ((size_t)y * (size_t)e->width + (size_t)x) * 3;
    e->rgb[idx] = (uint8_t)(color >> 16);
    e->rgb[idx + 1] = (uint8_t)(color >> 8);
    e->rgb[idx + 2] = (uint8_t)color;
}
void mw_clear(MW_Engine *e, uint32_t color) {
    if (!e) return;
    for (int y = 0; y < e->height; y++)
        for (int x = 0; x < e->width; x++) mw_pixel(e, x, y, color);
}
void mw_rect(MW_Engine *e, int x, int y, int w, int h, uint32_t color) {
    if (!e || w <= 0 || h <= 0) return;
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int right = x + w > e->width ? e->width : x + w;
    int bottom = y + h > e->height ? e->height : y + h;
    for (int yy = top; yy < bottom; yy++)
        for (int xx = left; xx < right; xx++) mw_pixel(e, xx, yy, color);
}
void mw_line(MW_Engine *e, int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        mw_pixel(e, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int twice = 2 * err;
        if (twice >= dy) { err += dy; x1 += sx; }
        if (twice <= dx) { err += dx; y1 += sy; }
    }
}
int mw_save_ppm(const MW_Engine *e, const char *filename) {
    if (!e || !filename) return 0;
    FILE *f = fopen(filename, "wb");
    if (!f) return 0;
    int ok = fprintf(f, "P6\n%d %d\n255\n", e->width, e->height) > 0;
    size_t size = (size_t)e->width * (size_t)e->height * 3;
    if (ok) ok = fwrite(e->rgb, 1, size, f) == size;
    if (fclose(f)) ok = 0;
    return ok;
}
uint32_t mw_checksum(const MW_Engine *e) {
    if (!e) return 0;
    uint32_t hash = 2166136261u;
    for (size_t i = 0, n = (size_t)e->width * (size_t)e->height * 3; i < n; i++) {
        hash ^= e->rgb[i]; hash *= 16777619u;
    }
    return hash;
}
