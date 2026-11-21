#include <styleos/window_service.h>
static struct ws_service global_windows;
static bool window_global_ready;
struct ws_service *ws_global(void)
{
    if (!window_global_ready) {
        ws_init(&global_windows);
        window_global_ready = true;
    }
    return &global_windows;
}
static void clear(void *p,size_t bytes)
{
    unsigned char *b=p;
    for(size_t i=0;i<bytes;++i)b[i]=0;
}
static int label_ok(const char *s)
{
    if(!s)return 0;
    for(unsigned i=0;i<WS_TITLE_BYTES;++i){
        unsigned char c=(unsigned char)s[i];
        if(!c)return i>0;
        if(c<0x20||c==0x7f)return 0;
    }
    return 0;
}
static int rect_ok(struct ws_bounds b)
{
    return b.width>0 && b.height>0 && b.width<=8192 && b.height<=8192 &&
           b.x>=-8192 && b.x<=8192 && b.y>=-8192 && b.y<=8192;
}
static struct ws_window *lookup(struct ws_service *ws,uint32_t id)
{
    if(!ws||!id)return 0;
    for(unsigned i=0;i<WS_MAX_WINDOWS;++i)
        if(ws->windows[i].id==id)return &ws->windows[i];
    return 0;
}
const struct ws_window *ws_find(const struct ws_service *ws,uint32_t id)
{ return lookup((struct ws_service *)ws,id); }
void ws_init(struct ws_service *ws)
{
    if(!ws)return;
    clear(ws,sizeof(*ws));
    ws->next_id=1;
    ws->compositor_generation=1;
}
static enum ws_status create(struct ws_service *ws, const char *title,
                             struct ws_bounds bounds,uint32_t owner,
                             enum ws_role role,enum ws_install_state status,
                             uint32_t *id)
{
    if(!ws||!id||!label_ok(title)||!rect_ok(bounds))return WS_INVALID;
    if(!ws->next_id||ws->next_id==UINT32_MAX)return WS_FULL;
    struct ws_window *free_window=0;
    for(unsigned i=0;i<WS_MAX_WINDOWS;++i)
        if(!ws->windows[i].id){free_window=&ws->windows[i];break;}
    if(!free_window)return WS_FULL;
    clear(free_window,sizeof(*free_window));
    free_window->id=ws->next_id++;
    free_window->owner_pid=owner;
    free_window->role=role;
    free_window->install_state=status;
    free_window->bounds=bounds;
    free_window->visible=true;
    free_window->install_enabled=status==WS_INSTALL_REVIEWED;
    free_window->error_code=status==WS_INSTALL_INCOMPATIBLE ? "TEA_INCOMPATIBLE" : 0;
    for(unsigned i=0;i<WS_TITLE_BYTES;++i){
        free_window->title[i]=title[i];
        if(!title[i])break;
    }
    *id=free_window->id;
    return WS_OK;
}
enum ws_status ws_open_app(struct ws_service *ws,const struct pr_table *p,
                           uint32_t owner,const char *title,
                           struct ws_bounds b,uint32_t *id)
{
    const struct pr_entry *entry=pr_find(p,owner);
    if(!entry||entry->type!=PR_NATIVE_USER||!pr_is_live(p,owner))return WS_DENIED;
    return create(ws,title,b,owner,WS_APP_WINDOW,0,id);
}
enum ws_status ws_open_package_dialog(struct ws_service *ws,const char *title,
                                      enum ws_install_state verdict,
                                      struct ws_bounds b,uint32_t *id)
{
    if(verdict!=WS_INSTALL_REVIEWED && verdict!=WS_INSTALL_INCOMPATIBLE)return WS_INVALID;
    return create(ws,title,b,0,WS_INSTALL_DIALOG,verdict,id);
}
enum ws_status ws_set_minimized(struct ws_service *ws,uint32_t requester,
                                uint32_t id,bool minimized)
{
    struct ws_window *w=lookup(ws,id);
    if(!w)return WS_MISSING;
    if(w->role!=WS_APP_WINDOW || !requester || w->owner_pid!=requester)return WS_DENIED;
    w->minimized=minimized;
    return WS_OK;
}
enum ws_status ws_close_app(struct ws_service *ws,uint32_t requester,uint32_t id)
{
    struct ws_window *w=lookup(ws,id);
    if(!w)return WS_MISSING;
    if(w->role!=WS_APP_WINDOW || !requester || w->owner_pid!=requester)return WS_DENIED;
    clear(w,sizeof(*w));
    return WS_OK;
}
enum ws_status ws_close_system(struct ws_service *ws,uint32_t id)
{
    struct ws_window *w=lookup(ws,id);
    if(!w)return WS_MISSING;
    if(w->role!=WS_INSTALL_DIALOG)return WS_DENIED;
    clear(w,sizeof(*w));
    return WS_OK;
}
void ws_reap_dead_owners(struct ws_service *ws,const struct pr_table *p)
{
    if(!ws||!p)return;
    for(unsigned i=0;i<WS_MAX_WINDOWS;++i){
        struct ws_window *w=&ws->windows[i];
        if(w->id && w->role==WS_APP_WINDOW && !pr_is_live(p,w->owner_pid))
            clear(w,sizeof(*w));
    }
}
uint32_t ws_compositor_restart(struct ws_service *ws)
{
    if(!ws||ws->compositor_generation==UINT32_MAX)return 0;
    return ++ws->compositor_generation;
}
size_t ws_snapshot(const struct ws_service *ws,struct ws_window *out,size_t cap)
{
    if(!ws||(cap&&!out))return 0;
    size_t count=0;
    for(unsigned i=0;i<WS_MAX_WINDOWS;++i)
        if(ws->windows[i].id){if(count<cap)out[count]=ws->windows[i];++count;}
    return count;
}
