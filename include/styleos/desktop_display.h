#ifndef STYLEOS_DESKTOP_DISPLAY_H
#define STYLEOS_DESKTOP_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

enum desktop_pixel_format {
    DESKTOP_RGB_BYTES = 0,
    DESKTOP_BGR_BYTES = 1,
};

struct desktop_surface {
    uintptr_t base;
    uint64_t bytes;
    uint32_t width, height, stride;
    enum desktop_pixel_format format;
};

/* Test profile only. A validated span is not proof of arbitrary MMIO access. */
bool desktop_surface_valid(const struct desktop_surface *);
struct boot_memory_evidence;
bool desktop_surface_excludes_boot_memory(const struct desktop_surface *,
                                           const struct boot_memory_evidence *);
bool desktop_draw_check(const struct desktop_surface *);
const struct desktop_surface *desktop_platform_surface(void);

#endif
