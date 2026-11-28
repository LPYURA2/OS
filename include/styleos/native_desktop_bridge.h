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
    struct ws_bounds bounds; /* unclipped geometry: same clip can hide a move */
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
/* raised is compositor view state, never a process or channel identity. */
struct ndb_input { uint32_t focus,capture,raised; };
unsigned ndb_track_top(const struct nwc_service *,struct ndb_tracker *,int,int,
                       uint32_t,struct desk_damage *);
unsigned ndb_paint_top(const struct nwc_service *,int,int,uint32_t,
                       ndb_pixel_plot,ndb_rect_plot,void *);
uint32_t ndb_hit_top(const struct nwc_service *,int,int,uint32_t,int,int);
uint32_t ndb_hit(const struct nwc_service *,int width,int height,int x,int y);
bool ndb_route_pointer(struct nwc_service *,struct ndb_input *,int width,int height,
                        int x,int y,uint32_t buttons,uint32_t modifiers,
                        bool pressed,bool released,bool blocked);
bool ndb_route_key(struct nwc_service *,struct ndb_input *,int width,int height,
                    uint32_t key,uint32_t modifiers,bool blocked);
/* Trusted shell operations, restricted to current visible native ports.
 * Closing is cooperative Alt+F4 delivery, never forced termination.
 * No focus mutation while a pointer capture is in progress. */
enum nwc_status ndb_request_close(struct nwc_service *,const struct ndb_input *,
                                 int width,int height,uint32_t expected_window);
bool ndb_cycle_focus(const struct nwc_service *,struct ndb_input *,int width,int height);
/* Panel layout is shared by renderer and hit testing. Keep the historic
 * five-slot geometry until more slots are needed; never overlap tray. */
enum { NDB_LEGACY_TASKS=5 };
struct ndb_task_panel {
    struct nwc_task tasks[NWC_PORTS];
    unsigned count,legacy,slots;
    uint32_t focus;
};
struct desk_box ndb_task_box(int width,int height,unsigned slots,unsigned index);
bool ndb_task_panel_update(const struct nwc_service *,struct ndb_task_panel *,
                           int width,int height,unsigned legacy,uint32_t focus,
                           struct desk_damage *);
const struct nwc_task *ndb_task_hit(const struct ndb_task_panel *,int width,int height,
                                    unsigned legacy,int x,int y);
/* Exact live token comparison for an in-progress trusted pointer capture.
 * The source must be visible, the target may be minimized. */
bool ndb_task_get(const struct nwc_service *,uint32_t window,struct nwc_task *);
bool ndb_task_equal(const struct nwc_task *,const struct nwc_task *);
bool ndb_task_current(const struct nwc_service *,const struct nwc_task *);
struct desk_box ndb_task_hover_box(const struct ndb_task_panel *,int,int,
                                    const struct nwc_task *);
#endif
