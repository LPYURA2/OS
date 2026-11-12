#ifndef MW_ENGINE_H
#define MW_ENGINE_H
#include <stdint.h>
#ifdef _WIN32
  #ifdef MW_BUILD
    #define MW_API __declspec(dllexport)
  #else
    #define MW_API __declspec(dllimport)
  #endif
#else
  #define MW_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct MW_Engine MW_Engine;
MW_API MW_Engine *mw_create(int width, int height);
MW_API void mw_destroy(MW_Engine *engine);
MW_API void mw_clear(MW_Engine *engine, uint32_t rgb);
MW_API void mw_pixel(MW_Engine *engine, int x, int y, uint32_t rgb);
MW_API void mw_rect(MW_Engine *engine, int x, int y, int w, int h, uint32_t rgb);
MW_API void mw_line(MW_Engine *engine, int x1, int y1, int x2, int y2, uint32_t rgb);
MW_API void mw_scene(MW_Engine *engine, uint32_t frame);
MW_API int mw_save_ppm(const MW_Engine *engine, const char *filename);
MW_API uint32_t mw_checksum(const MW_Engine *engine);
MW_API int mw_width(const MW_Engine *engine);
MW_API int mw_height(const MW_Engine *engine);
#ifdef __cplusplus
}
#endif
#endif
