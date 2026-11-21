#include <styleos/native_window_channel.h>

static struct nwc_service global_channel;
static void zero(void *ptr,size_t size)
{
    unsigned char *p=(unsigned char *)ptr;
    for(size_t i=0;i<size;++i)p[i]=0;
}
static bool live(const struct pr_entry *p)
{
    return p && p->type==PR_NATIVE_USER &&
           (p->state==PR_READY || p->state==PR_RUNNING || p->state==PR_WAITING);
}
static const struct usr_owner *binding(const struct usr_runtime *s,uint32_t pid)
{
    if(!s || !pid)return 0;
    for(unsigned i=0;i<PR_MAX_PROCESSES;++i)
        if(s->owners[i].pid==pid)return &s->owners[i];
    return 0;
}
static bool consistent(const struct nwc_service *s)
{
    return s && s->ready && s->registry && s->sessions && s->windows &&
           s->sessions->ready && s->sessions->processes==s->registry &&
           s->sessions->windows==s->windows;
}
static const struct nwc_port *find_port(const struct nwc_service *s,uint32_t win)
{
    if(!s || !win)return 0;
    for(unsigned i=0;i<NWC_PORTS;++i)
        if(s->ports[i].window_id==win)return &s->ports[i];
    return 0;
}
static struct nwc_port *mut_port(struct nwc_service *s,uint32_t win)
{ return (struct nwc_port *)find_port(s,win); }
static bool valid_port(const struct nwc_service *s,const struct nwc_port *p)
{
    if(!consistent(s) || !p || !p->window_id)return false;
    const struct pr_entry *proc=pr_find(s->registry,p->owner_pid);
    const struct usr_owner *own=binding(s->sessions,p->owner_pid);
    const struct ws_window *win=ws_find(s->windows,p->window_id);
    const struct usr_session *session=usr_find(s->sessions,p->session_id);
    return live(proc) && proc->scheduler_handle==p->scheduler_handle &&
           proc->page_root==p->user_root && own &&
           own->scheduler_handle==p->scheduler_handle &&
           own->session_id==p->session_id && session && win &&
           win->role==WS_APP_WINDOW && win->owner_pid==p->owner_pid;
}
static bool selected(const struct nwc_service *s,const struct nwc_port *p)
{
    if(!valid_port(s,p) || !s->sessions->selected_session ||
       s->sessions->selected_session!=p->session_id)return false;
    const struct usr_session *sess=usr_find(s->sessions,p->session_id);
    return sess && sess->state==USR_ACTIVE;
}
static bool authentic(const struct nwc_port *p,uint32_t pid,uint64_t handle,
                      uint64_t root)
{
    return p && pid && handle && root && !(root&UINT64_C(4095)) &&
           p->owner_pid==pid && p->scheduler_handle==handle &&
           p->user_root==root;
}
enum nwc_status nwc_start(struct nwc_service *s,const struct pr_table *registry,
                          const struct usr_runtime *sessions,
                          const struct ws_service *windows)
{
    if(!s || !registry || !sessions || !windows)return NWC_INVALID;
    if(s->ready)return NWC_STALE;
    if(!sessions->ready || sessions->processes!=registry ||
       sessions->windows!=windows ||
       pr_snapshot(registry,0,0,true)!=0 ||
       ws_snapshot(windows,0,0)!=0)return NWC_NOT_READY;
    zero(s,sizeof(*s));
    s->registry=registry;s->sessions=sessions;s->windows=windows;s->ready=true;
    return NWC_OK;
}
enum nwc_status nwc_claim(struct nwc_service *s,uint32_t pid,uint64_t handle,
                          uint64_t root,uint32_t win)
{
    if(!consistent(s))return NWC_NOT_READY;
    if(!pid || !handle || !root || (root&UINT64_C(4095)) || !win)
        return NWC_INVALID;
    if(find_port(s,win))return NWC_STALE;
    const struct pr_entry *proc=pr_find(s->registry,pid);
    const struct usr_owner *own=binding(s->sessions,pid);
    const struct ws_window *window=ws_find(s->windows,win);
    if(!live(proc) || proc->scheduler_handle!=handle ||
       proc->page_root!=root || !own || own->scheduler_handle!=handle ||
       !window || window->role!=WS_APP_WINDOW || window->owner_pid!=pid)
        return NWC_DENIED;
    const struct usr_session *sess=usr_find(s->sessions,own->session_id);
    if(!sess || sess->state!=USR_ACTIVE ||
       s->sessions->selected_session!=sess->id)return NWC_DENIED;
    struct nwc_port *port=0;
    for(unsigned i=0;i<NWC_PORTS;++i)
        if(!s->ports[i].window_id){port=&s->ports[i];break;}
    if(!port)return NWC_FULL;
    zero(port,sizeof(*port));
    port->window_id=win;port->owner_pid=pid;port->session_id=own->session_id;
    port->scheduler_handle=handle;port->user_root=root;port->next_sequence=1;
    return NWC_OK;
}
static enum nwc_status enqueue(struct nwc_service *s,uint32_t win,
                               const struct nwc_event *event)
{
    if(!consistent(s))return NWC_NOT_READY;
    struct nwc_port *p=mut_port(s,win);
    if(!p)return NWC_MISSING;
    if(!selected(s,p))return NWC_DENIED;
    if(p->length==NWC_EVENTS)return NWC_FULL;
    if(!p->next_sequence || p->next_sequence==UINT32_MAX)return NWC_OVERFLOW;
    struct nwc_event *out=&p->events[(p->head+p->length)%NWC_EVENTS];
    *out=*event;out->sequence=p->next_sequence++;out->window_id=win;
    ++p->length;
    return NWC_OK;
}
enum nwc_status nwc_send_key(struct nwc_service *s,uint32_t win,
                             uint32_t code,uint32_t modifiers,bool down)
{
    if(!win || !code || code>0x10ffffu || (modifiers&~15u))return NWC_INVALID;
    struct nwc_event ev={0};
    ev.kind=NWC_KEY;ev.code=code;ev.modifiers=modifiers;ev.down=down;
    return enqueue(s,win,&ev);
}
enum nwc_status nwc_send_text(struct nwc_service *s,uint32_t win,
                              const uint8_t *src,size_t len)
{
    if(!win || !src || !len || len>NWC_TEXT_BYTES)return NWC_INVALID;
    struct nwc_event ev={0};ev.kind=NWC_TEXT;ev.size=(uint8_t)len;
    for(size_t i=0;i<len;++i)ev.bytes[i]=src[i];
    return enqueue(s,win,&ev);
}
enum nwc_status nwc_receive(struct nwc_service *s,uint32_t pid,uint64_t handle,
                            uint64_t root,uint32_t win,struct nwc_event *out)
{
    if(!out || !pid || !handle || !root || !win)return NWC_INVALID;
    if(!consistent(s))return NWC_NOT_READY;
    struct nwc_port *p=mut_port(s,win);
    if(!p)return NWC_MISSING;
    if(!authentic(p,pid,handle,root) || !selected(s,p))return NWC_DENIED;
    if(!p->length)return NWC_EMPTY;
    *out=p->events[p->head];
    zero(&p->events[p->head],sizeof(p->events[p->head]));
    p->head=(p->head+1)%NWC_EVENTS;--p->length;
    return NWC_OK;
}
enum nwc_status nwc_publish_tile(struct nwc_service *s,uint32_t pid,
                                 uint64_t handle,uint64_t root,uint32_t win,
                                 const uint32_t *pixels,unsigned w,unsigned h)
{
    if(!pixels || !pid || !handle || !root || !win || !w || !h ||
       w>NWC_TILE_WIDTH || h>NWC_TILE_HEIGHT)return NWC_INVALID;
    if(!consistent(s))return NWC_NOT_READY;
    struct nwc_port *p=mut_port(s,win);
    if(!p)return NWC_MISSING;
    if(!authentic(p,pid,handle,root) || !selected(s,p))return NWC_DENIED;
    if(p->has_tile && p->tile.generation==UINT32_MAX)return NWC_OVERFLOW;
    /* Use an all-zero fresh tile. A smaller update cannot expose old pixels. */
    struct nwc_tile tile={0};
    tile.window_id=win;tile.width=(uint16_t)w;tile.height=(uint16_t)h;
    tile.generation=p->has_tile?p->tile.generation+1u:1u;
    for(size_t i=0;i<(size_t)w*h;++i)tile.pixel[i]=pixels[i]&UINT32_C(0x00ffffff);
    p->tile=tile;p->has_tile=true;
    return NWC_OK;
}
enum nwc_status nwc_visible_tile(const struct nwc_service *s,uint32_t win,
                                 struct nwc_tile *out)
{
    if(!out || !win)return NWC_INVALID;
    if(!consistent(s))return NWC_NOT_READY;
    const struct nwc_port *p=find_port(s,win);
    if(!p)return NWC_MISSING;
    if(!selected(s,p))return NWC_DENIED;
    if(!p->has_tile)return NWC_EMPTY;
    *out=p->tile;return NWC_OK;
}
size_t nwc_reconcile(struct nwc_service *s)
{
    if(!consistent(s))return 0;
    size_t count=0;
    for(unsigned i=0;i<NWC_PORTS;++i)
        if(s->ports[i].window_id && !valid_port(s,&s->ports[i])){
            zero(&s->ports[i],sizeof(s->ports[i]));++count;
        }
    return count;
}
size_t nwc_port_count(const struct nwc_service *s)
{
    if(!consistent(s))return 0;
    size_t total=0;
    for(unsigned i=0;i<NWC_PORTS;++i)if(s->ports[i].window_id)++total;
    return total;
}
enum nwc_status nwc_global_start(void)
{
    return nwc_start(&global_channel,pr_global(),usr_global(),ws_global());
}
const struct nwc_service *nwc_global(void)
{
    return global_channel.ready?&global_channel:0;
}
