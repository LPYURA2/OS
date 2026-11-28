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
#endif
