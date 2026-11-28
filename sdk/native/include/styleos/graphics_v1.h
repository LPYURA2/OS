#ifndef STYLEOS_SDK_GRAPHICS_V1_H
#define STYLEOS_SDK_GRAPHICS_V1_H
#include <styleos/native_v1.h>
/* App-owned retained rectangle buffer, no allocation, maximum 32 rects.
 * First rectangle is opaque background. Put this structure in private
 * DATA (not stack) when publishing. No kernel layout dependency. */
struct sty_canvas {
 uint32_t width,height,count;
 struct sty_rect rect[STY_FRAME_RECTS];
};
enum sty_draw_result { STY_DRAW_OK,STY_DRAW_INVALID,STY_DRAW_FULL };
bool sty_canvas_begin(struct sty_canvas *,uint32_t width,uint32_t height,uint32_t rgb);
enum sty_draw_result sty_canvas_rect(struct sty_canvas *,int32_t x,int32_t y,
 uint32_t width,uint32_t height,uint32_t rgb);
uint64_t sty_canvas_publish(uint32_t window,const struct sty_canvas *);
/* Strict UTF-8: returns false without modifying outputs on malformed,
 * truncated, overlong, surrogate or out-of-range input. */
bool sty_utf8_next(const uint8_t *,size_t bytes,uint32_t *codepoint,size_t *used);
/* Existing desktop 16px bitmap font, Latin and Cyrillic subset; absent
 * valid Unicode scalars use '?'. Not shaping or general Unicode fonts. */
bool sty_glyph_advance(uint32_t codepoint,uint32_t *advance);
bool sty_text_measure(const uint8_t *,size_t bytes,uint32_t *width);
/* Local, app-owned XRGB8888 raster. No shared memory or compositor authority.
 * Storage supplied by caller, initialized by begin. Max 64 KiB, including
 * row padding, for this initial SDK. A small viewport/strip is intentional.
 * Dirty bounds are a conservative union of actually modified pixels. */
#define STY_SURFACE_PIXELS 16384u
struct sty_damage { uint32_t x,y,w,h; };
struct sty_surface {
 uint32_t *pixels;
 size_t capacity;
 uint32_t width,height,stride;
 struct sty_damage dirty;
};
bool sty_surface_begin(struct sty_surface *,uint32_t *pixels,size_t capacity,
 uint32_t width,uint32_t height,uint32_t stride,uint32_t rgb);
bool sty_surface_fill(struct sty_surface *,int32_t x,int32_t y,
 uint32_t width,uint32_t height,uint32_t rgb);
bool sty_surface_text(struct sty_surface *,int32_t x,int32_t y,
 const uint8_t *utf8,size_t bytes,uint32_t rgb);
/* Only clear after consumer has successfully copied dirty data; caller
 * serializes drawing and consumption. Does not publish anything to kernel. */
bool sty_surface_damage(const struct sty_surface *,struct sty_damage *);
bool sty_surface_clear_damage(struct sty_surface *);
#endif
