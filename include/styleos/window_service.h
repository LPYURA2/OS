#ifndef STYLEOS_WINDOW_SERVICE_H
#define STYLEOS_WINDOW_SERVICE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <styleos/process_runtime.h>
enum { WS_MAX_WINDOWS = 24, WS_TITLE_BYTES = 80 };
enum ws_role { WS_APP_WINDOW = 1, WS_INSTALL_DIALOG = 2 };
enum ws_install_state { WS_INSTALL_REVIEWED = 1, WS_INSTALL_INCOMPATIBLE = 2 };
enum ws_status { WS_OK = 0, WS_INVALID, WS_DENIED, WS_FULL, WS_MISSING };
struct ws_bounds { int x,y,width,height; };
struct ws_window {
    uint32_t id, owner_pid;
    enum ws_role role;
    enum ws_install_state install_state;
    struct ws_bounds bounds;
    bool visible, minimized, install_enabled;
    char title[WS_TITLE_BYTES];
    const char *error_code; /* static UI contract; never user-controlled */
};
struct ws_service {
    uint32_t next_id, compositor_generation;
    struct ws_window windows[WS_MAX_WINDOWS];
};
void ws_init(struct ws_service *ws);
/* System-owned window state survives compositor restart. */
struct ws_service *ws_global(void);
enum ws_status ws_open_app(struct ws_service *ws,const struct pr_table *processes,
                           uint32_t owner_pid,const char *title,
                           struct ws_bounds bounds,uint32_t *window_id);
/* Trusted installer supplies a validated verdict. This only models a modal;
 * it does not grant permissions or install a package. */
enum ws_status ws_open_package_dialog(struct ws_service *ws, const char *title,
                                      enum ws_install_state verdict,
                                      struct ws_bounds bounds,uint32_t *window_id);
enum ws_status ws_set_minimized(struct ws_service *ws,uint32_t requester_pid,
                                uint32_t window_id,bool minimized);
enum ws_status ws_close_app(struct ws_service *ws,uint32_t requester_pid,
                            uint32_t window_id);
enum ws_status ws_close_system(struct ws_service *ws,uint32_t window_id);
const struct ws_window *ws_find(const struct ws_service *ws,uint32_t id);
void ws_reap_dead_owners(struct ws_service *ws,const struct pr_table *processes);
uint32_t ws_compositor_restart(struct ws_service *ws);
size_t ws_snapshot(const struct ws_service *ws,struct ws_window *out,size_t cap);
#endif
