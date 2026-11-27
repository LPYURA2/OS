#ifndef STYLEOS_NATIVE_WINDOW_CHANNEL_H
#define STYLEOS_NATIVE_WINDOW_CHANNEL_H

#include <styleos/deep_drop_abi.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <styleos/user_session_runtime.h>

/* Kernel-owned staging for future bounded window syscalls/IPC. There is NO
 * user-pointer copying, trap entry, actual pixel compositor or user process
 * in the ordinary desktop profile yet. A trusted arch broker must first copy
 * untrusted bytes into kernel-owned memory and authenticate the CPU context. */
enum { NWC_PORTS = 8, NWC_EVENTS = 4, NWC_TEXT_BYTES = 32,
       NWC_TILE_WIDTH = 16, NWC_TILE_HEIGHT = 16,
       NWC_TILE_PIXELS = NWC_TILE_WIDTH * NWC_TILE_HEIGHT };

enum nwc_status { NWC_OK = 0, NWC_INVALID, NWC_NOT_READY,
                  NWC_DENIED, NWC_MISSING, NWC_FULL, NWC_EMPTY,
                  NWC_STALE, NWC_OVERFLOW };
enum nwc_event_kind { NWC_KEY = 1, NWC_TEXT = 2, NWC_POINTER = 3, NWC_DEEP_DROP = DD_EVENT_KIND };
struct nwc_event {
    uint32_t sequence, window_id;
    enum nwc_event_kind kind;
    uint32_t code, modifiers;
    bool down;
    uint8_t size, bytes[NWC_TEXT_BYTES];
};
/* Exactly one small XRGB8888 tile per independently owned window. This is a
 * proof-of-ownership buffer, not yet a graphical window or a framebuffer. */
struct nwc_tile {
    uint32_t window_id, generation;
    uint16_t width, height;
    uint32_t pixel[NWC_TILE_PIXELS];
};
/* Bounded retained opaque frame. First rectangle covers the entire
 * window, subsequent rectangles overlay it. No app-controlled pointers. */
enum { NWC_FRAME_RECTS=32 };
struct nwc_rect { uint32_t x,y,w,h,rgb; };
_Static_assert(sizeof(struct nwc_rect)==20,"rectangle wire extent");
struct nwc_frame {
    uint32_t window_id,generation,count;
    struct nwc_rect rect[NWC_FRAME_RECTS];
};
struct nwc_port {
    uint32_t window_id, owner_pid, session_id;
    uint64_t scheduler_handle, user_root;
    uint32_t next_sequence;
    struct nwc_event events[NWC_EVENTS];
    unsigned head, length;
    struct nwc_tile tile;
    bool has_tile;
    struct nwc_frame frame;
};
struct nwc_service {
    const struct pr_table *registry;
    const struct usr_runtime *sessions;
    const struct ws_service *windows;
    struct nwc_port ports[NWC_PORTS];
    bool ready;
};

enum nwc_status nwc_start(struct nwc_service *,const struct pr_table *,
                          const struct usr_runtime *,const struct ws_service *);
/* Only the trusted launch broker may claim a port after the PID, page root,
 * authenticated session and WS window have already been established. */
enum nwc_status nwc_claim(struct nwc_service *,uint32_t pid,uint64_t handle,
                          uint64_t observed_user_root,uint32_t window_id);
/* Trusted window manager posts to the ACTIVE session only. Never callable
 * directly from user programs. An event is copied into a bounded queue. */
enum nwc_status nwc_send_key(struct nwc_service *,uint32_t window_id,
                             uint32_t keycode,uint32_t modifiers,bool pressed);
enum nwc_status nwc_send_text(struct nwc_service *,uint32_t window_id,
                              const uint8_t *kernel_copy,size_t size);
/* Queue only the capability id. The runtime audits the complete record. */
enum nwc_status nwc_send_drop(struct nwc_service *,uint32_t window,uint32_t code,uint64_t id);
bool nwc_drop_event_valid(const struct nwc_event *);
/* Trusted pointer producer: code = button mask, down = primary button,
 * size = 8, payload = two little-endian signed 32-bit window-relative
 * coordinates (may be outside the window during capture). */
enum nwc_status nwc_send_pointer(struct nwc_service *,uint32_t,int32_t,int32_t,
                                 uint32_t buttons,uint32_t modifiers);
/* Trusted CPL3 trap layer must authenticate all PID/handle/root parameters
 * against the actual CPU context; this API does NOT establish trap identity. */
enum nwc_status nwc_receive(struct nwc_service *,uint32_t pid,uint64_t handle,
                            uint64_t observed_user_root,uint32_t window_id,
                            struct nwc_event *kernel_output);
enum nwc_status nwc_publish_tile(struct nwc_service *,uint32_t pid,uint64_t handle,
                                 uint64_t observed_user_root,uint32_t window_id,
                                 const uint32_t *kernel_pixels,
                                 unsigned width,unsigned height);
/* Compositor-only snapshot. Hidden, locked and foreign windows are denied. */
/* x86 register-only probe ABI: RAX=operation, RBX=window (0 selects
 * the caller's unique valid port), RCX=24-bit RGB; RAX returns nwc_status.
 * Only the architecture's authenticated trap dispatcher supplies identity.
 * No raw pointers, no implicit window creation, no cross-owner authority. */
#define NWC_CALL_PIXEL UINT64_C(0x100)
/* RAX=0x101, RBX=window, RCX=user data VA, RDX=height<<16|width.
 * 1..16 pixels per dimension, row-major little-endian RGB24 in u32.
 * The architecture copies the complete bounded buffer before calling. */
#define NWC_CALL_TILE UINT64_C(0x101)
/* RAX=0x102, RBX=window (0 unique port), RCX=private writable data VA,
 * RDX=64. RAX returns nwc_status. Fixed little-endian wire, no C padding
 * or kernel pointers: version, sequence, window, kind, code, modifiers,
 * down, size (8 u32), then 32 bytes. Failed calls do not consume input. */
#define NWC_CALL_INPUT UINT64_C(0x102)
/* RAX=0x103, RBX=window (0 unique), RCX=private data VA,
 * RDX=count 1..32; five little-endian u32 per rectangle. Atomic replacement. */
#define NWC_CALL_FRAME UINT64_C(0x103)
enum nwc_status nwc_publish_frame(struct nwc_service *,uint32_t,uint64_t,uint64_t,
                                  uint64_t,const struct nwc_rect *,unsigned);
/* Serialized trusted compositor only, alias invalid after mutation. */
const struct nwc_frame *nwc_visible_frame(const struct nwc_service *,uint32_t);
bool nwc_frame_valid(const struct nwc_port *,const struct ws_window *);
/* Read-only condition for a trusted serialized wait broker. Same authority
 * checks as receive; no event consumption or output pointer. */
enum nwc_status nwc_input_ready(struct nwc_service *,uint32_t,uint64_t,uint64_t,uint64_t);
#define NWC_INPUT_BYTES 64u
enum nwc_status nwc_receive_wire(struct nwc_service *,uint32_t,uint64_t,uint64_t,
                                  uint64_t window,uint8_t out[NWC_INPUT_BYTES]);
enum nwc_status nwc_publish_block(struct nwc_service *,uint32_t,uint64_t,uint64_t,
                                  uint64_t window,const uint32_t *,unsigned,unsigned);
enum nwc_status nwc_publish_pixel(struct nwc_service *,uint32_t,uint64_t,uint64_t,
                                  uint64_t window,uint64_t rgb);
enum nwc_status nwc_visible_tile(const struct nwc_service *,uint32_t window_id,
                                 struct nwc_tile *kernel_output);
/* Trusted panel only. Snapshot includes minimized windows, never foreign,
 * locked, hidden or dead owners. Identity is internal (never user ABI).
 * Slots may move after close; use the entire observed token, not an index. */
struct nwc_task {
    uint32_t window_id,pid,session_id;
    uint64_t generation,root;
    bool minimized;
};
size_t nwc_task_snapshot(const struct nwc_service *,struct nwc_task *,size_t capacity);
/* Serialized safe-root compositor action; exact snapshot and current session
 * required, including the old minimized bit. No focus or event mutation. */
enum nwc_status nwc_task_set_minimized(struct nwc_service *,const struct nwc_task *,bool);
/* Discard stale ports after the process/window service reconciles exit/fault;
 * do not kill a process, delete a window or free physical RAM here. */
size_t nwc_reconcile(struct nwc_service *);
size_t nwc_port_count(const struct nwc_service *);
/* Empty singleton bootstrapped by ordinary x86 desktop. */
enum nwc_status nwc_global_start(void);
const struct nwc_service *nwc_global(void);
#endif
