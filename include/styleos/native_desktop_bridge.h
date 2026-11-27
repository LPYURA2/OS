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
typedef void (*ndb_rect_plot)(void *,int,int,int,int,uint32_t);
unsigned ndb_paint_rects(const struct nwc_service *,int,int,ndb_pixel_plot,ndb_rect_plot,void *);

/* Invalidate old and new clipped regions after a tile, selection, lock,
 * close, fault, move or minimize. The caller redraws the complete scene in
 * the damaged strips; no framebuffer backing is retained in this module. */
unsigned ndb_track(const struct nwc_service *ipc, struct ndb_tracker *tracker,
                   int width, int height, struct desk_damage *damage);
/* Clip to BOTH the owner window's bounds and the actual desktop work area. */
unsigned ndb_paint(const struct nwc_service *ipc, int width, int height,
                   ndb_pixel_plot plot, void *opaque);
/* Input follows the same clipped tile and paint order as presentation.
 * Only a primary press selects a new native target. Capture owns the
 * subsequent motion/release even after leaving its tile. No click-through
 * when the target queue is full or the captured window has disappeared.
 * Shell passes blocked for menus, panel, or an existing shell drag. */
struct ndb_input { uint32_t focus,capture; };
uint32_t ndb_hit(const struct nwc_service *,int width,int height,int x,int y);
bool ndb_route_pointer(struct nwc_service *,struct ndb_input *,int width,int height,
                        int x,int y,uint32_t buttons,uint32_t modifiers,
                        bool pressed,bool released,bool blocked);
bool ndb_route_key(struct nwc_service *,struct ndb_input *,int width,int height,
                    uint32_t key,uint32_t modifiers,bool blocked);
#endif
