#include "mw_engine.h"
#include <cmath>
#include <cstdint>
#include <vector>

// C++ scene layer, backed by an entirely C framebuffer and C ABI.
namespace {
struct Spark { int x; int y; int size; };
uint32_t mix(uint32_t a, uint32_t b, int t) {
    int r = (((a >> 16) & 255) * (255-t) + ((b >> 16) & 255) * t) / 255;
    int g = (((a >> 8) & 255) * (255-t) + ((b >> 8) & 255) * t) / 255;
    int bl = ((a & 255) * (255-t) + (b & 255) * t) / 255;
    return (uint32_t)((r << 16) | (g << 8) | bl);
}
}
extern "C" MW_API void mw_scene(MW_Engine *e, uint32_t frame) {
    if (!e) return;
    const int w = mw_width(e), h = mw_height(e);
    for (int y = 0; y < h; ++y) {
        int fade = (y * 235) / h;
        mw_rect(e, 0, y, w, 1, mix(0x090e20, 0x1b2440, fade));
    }
    std::vector<Spark> stars;
    uint32_t rng = 0x12345678;
    for (int i = 0; i < 90; ++i) {
        rng = rng * 1664525u + 1013904223u;
        stars.push_back({int(rng % uint32_t(w)), int((rng >> 16) % uint32_t(h)), int((rng & 3u) == 0u ? 2 : 1)});
    }
    for (const auto &s : stars) mw_rect(e, s.x, s.y, s.size, s.size, 0x748bb4);
    // Fake hardware monitor: motherboard, traces, chip and pulsing signal.
    int bx = w / 7, by = h / 6, bw = w * 5 / 7, bh = h * 2 / 3;
    mw_rect(e, bx, by, bw, bh, 0x172a31);
    mw_rect(e, bx + 3, by + 3, bw - 6, bh - 6, 0x102329);
    for (int i = 0; i < 11; i++) {
        int cx = bx + bw / 2 + (i - 5) * 14;
        mw_line(e, cx, by + 5, cx, by + bh - 5, 0x1c786b);
    }
    int chip = h / 3;
    int left = w/2 - chip/2, top = h/2 - chip/2;
    mw_rect(e, left - 7, top - 7, chip+14, chip+14, 0xa2c7bb);
    mw_rect(e, left, top, chip, chip, 0x07151e);
    mw_rect(e, left+8, top+8, chip-16, chip-16, 0x162a37);
    for (int n = 0; n < 12; n++) {
        int yy = top + (n+1)*chip/13;
        mw_rect(e, left-11, yy, 10, 3, 0xe1b66b);
        mw_rect(e, left+chip+1, yy, 10, 3, 0xe1b66b);
    }
    for (int n = 0; n < 12; n++) {
        int xx = left + (n+1)*chip/13;
        mw_rect(e, xx, top-11, 3, 10, 0xe1b66b);
        mw_rect(e, xx, top+chip+1, 3, 10, 0xe1b66b);
    }
    int pulse = int(frame % 70u);
    int py = top + chip/2;
    mw_line(e, left + 14, py, left + chip/2, py - (pulse - 35)/2, 0x36fcbd);
    mw_line(e, left + chip/2, py - (pulse - 35)/2, left + chip - 14, py, 0x36fcbd);
    mw_rect(e, 0, h-17, w, 17, 0x070b14);
    mw_rect(e, 9, h-10, (w-18)*int(frame%101u)/100, 3, 0x32dfae);
}
