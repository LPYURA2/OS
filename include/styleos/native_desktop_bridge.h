#ifndef STYLEOS_NATIVE_DESKTOP_BRIDGE_H
#define STYLEOS_NATIVE_DESKTOP_BRIDGE_H

#include <styleos/desktop_v0.h>
#include <styleos/native_window_channel.h>

/* Production Desktop presentation boundary. It does not start a process,
 * authenticate a trap or allocate a user page. Only kernel-owned tile copies
 * already authorized by native_window_channel are considered. */
struct ndb_seen {
    uint32_t window_id, generation;
    struct desk_box rect;
};
struct ndb_tracker {
    struct ndb_seen last[NWC_PORTS];
    unsigned count;
};
typedef void (*ndb_pixel_plot)(void *opaque, int x, int y, uint32_t rgb);

/* Invalidate old and new clipped regions after a tile, selection, lock,
 * close, fault, move or minimize. The caller redraws the complete scene in
 * the damaged strips; no framebuffer backing is retained in this module. */
unsigned ndb_track(const struct nwc_service *ipc, struct ndb_tracker *tracker,
                   int width, int height, struct desk_damage *damage);
/* Clip to BOTH the owner window's bounds and the actual desktop work area. */
unsigned ndb_paint(const struct nwc_service *ipc, int width, int height,
                   ndb_pixel_plot plot, void *opaque);
#endif
