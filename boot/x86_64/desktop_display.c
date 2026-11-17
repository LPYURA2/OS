#include "desktop_display.h"
#include <styleos/desktop_display.h>

typedef EFI_STATUS (EFIAPI *efi_locate_protocol)(struct efi_guid *, void *, void **);

struct desktop_efi_boot_services {
    struct efi_boot_services prefix;
    void *get_next_monotonic_count, *stall, *set_watchdog_timer;
    void *connect_controller, *disconnect_controller, *open_protocol;
    void *close_protocol, *open_protocol_information, *protocols_per_handle;
    void *locate_handle_buffer;
    efi_locate_protocol locate_protocol;
};

struct efi_gop_info {
    uint32_t version, horizontal, vertical, pixel_format;
    uint32_t pixel_masks[4], pixels_per_scan_line;
};

struct efi_gop_mode {
    uint32_t max_mode, mode;
    const struct efi_gop_info *info;
    UINTN info_size;
    uint64_t framebuffer_base;
    UINTN framebuffer_size;
};

struct efi_gop {
    void *query_mode, *set_mode, *blt;
    const struct efi_gop_mode *mode;
};

_Static_assert(offsetof(struct desktop_efi_boot_services, locate_protocol) == 320,
               "UEFI LocateProtocol offset");
_Static_assert(offsetof(struct efi_gop, mode) == 24, "UEFI GOP mode pointer");
_Static_assert(offsetof(struct efi_gop_mode, framebuffer_base) == 24,
               "UEFI GOP framebuffer base");
_Static_assert(sizeof(struct efi_gop_info) == 36, "UEFI GOP mode info");

static struct desktop_surface surface;

void desktop_capture_uefi_mode(struct efi_boot_services *bs)
{
    static struct efi_guid graphics_output = {
        0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
    };
    const struct desktop_efi_boot_services *extended = (const void *)bs;
    void *interface = NULL;
    if (!bs || bs->hdr.header_size < sizeof(*extended) ||
        !extended->locate_protocol ||
        extended->locate_protocol(&graphics_output, NULL, &interface) != EFI_SUCCESS ||
        !interface)
        return;
    const struct efi_gop_mode *mode = ((const struct efi_gop *)interface)->mode;
    if (!mode || mode->mode >= mode->max_mode || !mode->info ||
        mode->info_size < sizeof(*mode->info) || mode->info->version != 0 ||
        mode->info->pixel_format > DESKTOP_BGR_BYTES ||
        mode->framebuffer_base > UINTPTR_MAX)
        return;
    struct desktop_surface candidate = {
        (uintptr_t)mode->framebuffer_base, mode->framebuffer_size, mode->info->horizontal,
        mode->info->vertical, mode->info->pixels_per_scan_line,
        (enum desktop_pixel_format)mode->info->pixel_format,
    };
    if (desktop_surface_valid(&candidate))
        surface = candidate;
}

const struct desktop_surface *desktop_platform_surface(void)
{
    return &surface;
}
