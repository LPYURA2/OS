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
    const struct ws_window *win=ws_find(s->windows,p->window_id);
    return sess && sess->state==USR_ACTIVE && win &&
           win->visible && !win->minimized;
}
static bool task_allowed(const struct nwc_service *s,const struct nwc_port *p)
{
    if(!valid_port(s,p)||s->sessions->selected_session!=p->session_id)return false;
    const struct usr_session *u=usr_find(s->sessions,p->session_id);
    const struct ws_window *w=ws_find(s->windows,p->window_id);
    return u&&u->state==USR_ACTIVE&&w&&w->visible;
}
size_t nwc_task_snapshot(const struct nwc_service *s,struct nwc_task *out,size_t cap)
{
    if(!consistent(s)||(cap&&!out))return 0;
    size_t n=0;
    for(unsigned i=0;i<NWC_PORTS;++i){
        const struct nwc_port *p=&s->ports[i];
        if(!task_allowed(s,p))continue;
        const struct ws_window *w=ws_find(s->windows,p->window_id);
        if(n<cap)out[n]=(struct nwc_task){.window_id=p->window_id,.pid=p->owner_pid,
            .session_id=p->session_id,.generation=p->scheduler_handle,.root=p->user_root,
            .minimized=w->minimized};
        ++n;
    }
    return n;
}
enum nwc_status nwc_task_set_minimized(struct nwc_service *s,const struct nwc_task *t,bool minimized)
{
    if(!consistent(s))return NWC_NOT_READY;
    if(!t||!t->window_id||!t->pid||!t->session_id||!t->generation||!t->root)return NWC_INVALID;
    const struct nwc_port *p=find_port(s,t->window_id);
    if(!task_allowed(s,p))return NWC_DENIED;
    const struct ws_window *w=ws_find(s->windows,t->window_id);
    if(p->owner_pid!=t->pid||p->session_id!=t->session_id||
       p->scheduler_handle!=t->generation||p->user_root!=t->root||
       w->minimized!=t->minimized)return NWC_STALE;
    return ws_set_minimized(s->sessions->windows,t->pid,t->window_id,minimized)==WS_OK?
        NWC_OK:NWC_DENIED;
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
bool nwc_drop_event_valid(const struct nwc_event *event)
{
    if (!event || event->kind != NWC_DEEP_DROP || event->size != 8 ||
        (event->code != DD_REQUEST_TEXT && event->code != DD_TEXT_OFFER) ||
        event->down || event->modifiers) return false;
    uint8_t nonzero = 0;
    for (unsigned i = 0; i < 8; ++i) nonzero |= event->bytes[i];
    for (unsigned i = 8; i < NWC_TEXT_BYTES; ++i)
        if (event->bytes[i]) return false;
    return nonzero != 0;
}
enum nwc_status nwc_send_drop(struct nwc_service *s,uint32_t win,uint32_t code,uint64_t id)
{
    struct nwc_event event = {.kind=NWC_DEEP_DROP, .code=code, .size=8};
    for (unsigned i = 0; i < 8; ++i) event.bytes[i] = (uint8_t)(id >> (8*i));
    if (!win || !nwc_drop_event_valid(&event)) return NWC_INVALID;
    return enqueue(s,win,&event);
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
static enum nwc_status resolve_port(struct nwc_service *s,uint32_t pid,uint64_t handle,
                                    uint64_t root,uint64_t window,uint32_t *out)
{
    if(!pid || !handle || !root || (root&4095u) || window>UINT32_MAX)return NWC_INVALID;
    if(!consistent(s))return NWC_NOT_READY;
    uint32_t win=(uint32_t)window;
    if(!win){
        for(unsigned i=0;i<NWC_PORTS;++i){
            const struct nwc_port *p=&s->ports[i];
            if(!p->window_id || !authentic(p,pid,handle,root) || !valid_port(s,p))continue;
            if(win)return NWC_DENIED;
            win=p->window_id;
        }
        if(!win)return NWC_DENIED;
    }
    *out=win;
    return NWC_OK;
}
enum nwc_status nwc_input_ready(struct nwc_service *s,uint32_t pid,uint64_t handle,
                                uint64_t root,uint64_t window){
    uint32_t win=0;
    enum nwc_status r=resolve_port(s,pid,handle,root,window,&win);
    if(r!=NWC_OK)return r;
    const struct nwc_port *p=find_port(s,win);
    if(!p)return NWC_MISSING;
    if(!authentic(p,pid,handle,root)||!selected(s,p))return NWC_DENIED;
    return p->length?NWC_OK:NWC_EMPTY;
}
enum nwc_wait_condition nwc_wait_condition(struct nwc_service *s,uint32_t pid,uint64_t handle,
                                           uint64_t root,uint64_t window)
{
    uint32_t win=0;
    if(resolve_port(s,pid,handle,root,window,&win)!=NWC_OK)return NWC_WAIT_REVOKED;
    const struct nwc_port *p=find_port(s,win);
    if(!authentic(p,pid,handle,root)||!valid_port(s,p))return NWC_WAIT_REVOKED;
    if(!selected(s,p))return NWC_WAIT_BLOCKED;
    return p->length?NWC_WAIT_EVENT:NWC_WAIT_EMPTY;
}
static void wire_u32(uint8_t *p,uint32_t v)
{
    for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));
}
enum nwc_status nwc_send_pointer(struct nwc_service *s,uint32_t win,int32_t x,int32_t y,
                                 uint32_t buttons,uint32_t modifiers)
{
    if(!win || (buttons&~7u) || (modifiers&~15u))return NWC_INVALID;
    struct nwc_event ev={0};
    ev.kind=NWC_POINTER;ev.code=buttons;ev.modifiers=modifiers;
    ev.down=(buttons&1u)!=0;ev.size=8;
    wire_u32(ev.bytes,(uint32_t)x);
    wire_u32(ev.bytes+4,(uint32_t)y);
    return enqueue(s,win,&ev);
}
enum nwc_status nwc_receive_wire(struct nwc_service *s,uint32_t pid,uint64_t handle,
                                 uint64_t root,uint64_t window,uint8_t out[NWC_INPUT_BYTES])
{
    if(!out)return NWC_INVALID;
    uint32_t win=0;
    enum nwc_status r=resolve_port(s,pid,handle,root,window,&win);
    if(r!=NWC_OK)return r;
    struct nwc_event ev={0};
    r=nwc_receive(s,pid,handle,root,win,&ev);
    if(r!=NWC_OK)return r;
    uint8_t wire[NWC_INPUT_BYTES]={0};
    const uint32_t fields[]={1,ev.sequence,ev.window_id,(uint32_t)ev.kind,
                             ev.code,ev.modifiers,ev.down?1u:0u,ev.size};
    for(unsigned i=0;i<8;++i)wire_u32(wire+4*i,fields[i]);
    for(unsigned i=0;i<ev.size && i<NWC_TEXT_BYTES;++i)wire[32+i]=ev.bytes[i];
    for(unsigned i=0;i<NWC_INPUT_BYTES;++i)out[i]=wire[i];
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
    uint32_t prior=p->frame.count?p->frame.generation:p->has_tile?p->tile.generation:0;
    if(prior==UINT32_MAX)return NWC_OVERFLOW;
    /* Use an all-zero fresh tile. A smaller update cannot expose old pixels. */
    struct nwc_tile tile={0};
    tile.window_id=win;tile.width=(uint16_t)w;tile.height=(uint16_t)h;
    tile.generation=prior+1u;
    for(size_t i=0;i<(size_t)w*h;++i)tile.pixel[i]=pixels[i]&UINT32_C(0x00ffffff);
    zero(&p->frame,sizeof p->frame);
    zero(p->patches,sizeof p->patches);p->patch_latest=0;
    zero(p->text,sizeof p->text);p->text_count=0;p->patch_base=0;
    p->tile=tile;p->has_tile=true;
    return NWC_OK;
}
enum nwc_status nwc_publish_block(struct nwc_service *s,uint32_t pid,uint64_t handle,
                                  uint64_t root,uint64_t window,const uint32_t *pixels,
                                  unsigned w,unsigned h)
{
    if(!pid || !handle || !root || (root&4095u) || !pixels ||
       window>UINT32_MAX || !w || !h || w>NWC_TILE_WIDTH || h>NWC_TILE_HEIGHT)return NWC_INVALID;
    for(size_t i=0;i<(size_t)w*h;++i)
        if(pixels[i]>UINT32_C(0x00ffffff))return NWC_INVALID;
    uint32_t win=0;
    enum nwc_status r=resolve_port(s,pid,handle,root,window,&win);
    if(r!=NWC_OK)return r;
    return nwc_publish_tile(s,pid,handle,root,win,pixels,w,h);
}
enum nwc_status nwc_publish_pixel(struct nwc_service *s,uint32_t pid,uint64_t handle,
                                  uint64_t root,uint64_t window,uint64_t rgb)
{
    if(rgb>UINT32_C(0x00ffffff))return NWC_INVALID;
    uint32_t pixel=(uint32_t)rgb;
    return nwc_publish_block(s,pid,handle,root,window,&pixel,1,1);
}
static bool frame_rects(const struct nwc_rect *r,unsigned n,const struct ws_window *w)
{
    if(!r||!n||n>NWC_FRAME_RECTS||!w||w->bounds.width<=0||w->bounds.height<=0||
       w->bounds.width>8192||w->bounds.height>8192)return false;
    unsigned width=(unsigned)w->bounds.width,height=(unsigned)w->bounds.height;
    if(r[0].x||r[0].y||r[0].w!=width||r[0].h!=height)return false;
    for(unsigned i=0;i<n;++i){
        const struct nwc_rect *q=&r[i];
        if(!q->w||!q->h||q->x>=width||q->y>=height||q->w>width-q->x||
           q->h>height-q->y||q->rgb>0xffffffu)return false;
    }
    return true;
}
static bool patch_shape(const struct nwc_patch *q,unsigned width,unsigned height)
{
    return q->x<width&&q->y<height&&!(q->x%16u)&&!(q->y%16u)&&
        q->w==(width-q->x<16u?width-q->x:16u)&&
        q->h==(height-q->y<16u?height-q->y:16u);
}
static bool patches_valid(const struct nwc_port *p,unsigned width,unsigned height)
{
    if(p->patch_latest>NWC_PATCH_SLOTS||p->patch_base>p->frame.generation||
       (p->frame.count?(!p->patch_base):p->patch_base!=0))return false;
    unsigned count=0;
    for(unsigned i=0;i<NWC_PATCH_SLOTS;++i){
        const struct nwc_patch *q=&p->patches[i];
        if(!q->w){
            const unsigned char *z=(const void *)q;
            for(size_t k=0;k<sizeof *q;++k)if(z[k])return false;
            continue;
        }
        if(!patch_shape(q,width,height)||q->generation<=p->patch_base||
           q->generation>p->frame.generation)return false;
        ++count;
        for(unsigned j=0;j<i;++j)
            if(p->patches[j].w&&p->patches[j].x==q->x&&p->patches[j].y==q->y)return false;
        for(unsigned j=0;j<NWC_TILE_PIXELS;++j)
            if(j<q->w*q->h?q->pixel[j]>0xffffffu:q->pixel[j]!=0)return false;
    }
    return count?(p->patch_latest&&p->patches[p->patch_latest-1].w&&
        p->patches[p->patch_latest-1].generation==p->frame.generation):
        !p->patch_latest&&p->patch_base==p->frame.generation;
}
static bool text_run_valid(const struct nwc_text_run *q,unsigned width,unsigned height)
{
    if(!q->width||q->x>=width||q->width>width-q->x||q->y>=height||
       NWC_TEXT_HEIGHT>height-q->y||!q->count||q->count>NWC_TEXT_GLYPHS||
       q->foreground>0xffffffu||q->background>0xffffffu)return false;
    for(unsigned j=0;j<NWC_TEXT_GLYPHS;++j){
        uint32_t cp=q->cp[j];
        if(j>=q->count){if(cp)return false;}
        else if(cp<32u||(cp>=127u&&cp<=159u)||cp>0x10ffffu||
                (cp>=0xd800u&&cp<=0xdfffu))return false;
    }
    return true;
}
static bool text_valid(const struct nwc_port *p,unsigned width,unsigned height)
{
    if(p->text_count>NWC_TEXT_RUNS)return false;
    for(unsigned i=0;i<NWC_TEXT_RUNS;++i){
        if(i<p->text_count){
            if(!text_run_valid(&p->text[i],width,height))return false;
        }else{
            const unsigned char *z=(const void *)&p->text[i];
            for(size_t j=0;j<sizeof p->text[i];++j)if(z[j])return false;
        }
    }
    return true;
}
bool nwc_frame_valid(const struct nwc_port *p,const struct ws_window *w)
{
    if(!p||!w)return false;
    const struct nwc_frame *f=&p->frame;
    if(!f->count){
        const unsigned char *b=(const void *)f;
        for(size_t i=0;i<sizeof *f;++i)if(b[i])return false;
        return !p->text_count&&text_valid(p,0,0)&&patches_valid(p,0,0);
    }
    if(p->has_tile||!f->generation||f->window_id!=p->window_id||
       !frame_rects(f->rect,f->count,w))return false;
    const unsigned char *unused=(const void *)&f->rect[f->count];
    for(size_t i=0;i<(NWC_FRAME_RECTS-f->count)*sizeof f->rect[0];++i)
        if(unused[i])return false;
    return text_valid(p,(unsigned)w->bounds.width,(unsigned)w->bounds.height)&&
        patches_valid(p,(unsigned)w->bounds.width,(unsigned)w->bounds.height);
}
enum nwc_status nwc_publish_frame(struct nwc_service *s,uint32_t pid,uint64_t handle,
                                  uint64_t root,uint64_t window,const struct nwc_rect *rect,unsigned count)
{
    if(!rect||!count||count>NWC_FRAME_RECTS)return NWC_INVALID;
    uint32_t win=0;
    enum nwc_status status=resolve_port(s,pid,handle,root,window,&win);
    if(status!=NWC_OK)return status;
    struct nwc_port *p=mut_port(s,win);
    if(!p)return NWC_MISSING;
    if(!authentic(p,pid,handle,root)||!selected(s,p))return NWC_DENIED;
    if(!frame_rects(rect,count,ws_find(s->windows,win)))return NWC_INVALID;
    uint32_t prior=p->frame.count?p->frame.generation:p->has_tile?p->tile.generation:0;
    if(prior==UINT32_MAX)return NWC_OVERFLOW;
    struct nwc_frame next={0};
    next.window_id=win;next.generation=prior+1;next.count=count;
    for(unsigned i=0;i<count;++i)next.rect[i]=rect[i];
    zero(&p->tile,sizeof p->tile);p->has_tile=false;p->frame=next;
    zero(p->patches,sizeof p->patches);p->patch_latest=0;
    zero(p->text,sizeof p->text);p->text_count=0;p->patch_base=next.generation;
    return NWC_OK;
}
const struct nwc_frame *nwc_visible_frame(const struct nwc_service *s,uint32_t win)
{
    if(!consistent(s))return 0;
    const struct nwc_port *p=find_port(s,win);
    if(!p||!selected(s,p)||!p->frame.count||
       !nwc_frame_valid(p,ws_find(s->windows,win)))return 0;
    return &p->frame;
}
enum nwc_status nwc_publish_patch(struct nwc_service *s,uint32_t pid,uint64_t handle,
    uint64_t root,uint64_t window,uint32_t expected,uint32_t x,uint32_t y,
    const uint32_t *pixels,unsigned w,unsigned h)
{
    if(!pixels||!expected||!w||!h||w>16||h>16)return NWC_INVALID;
    uint32_t win=0;
    enum nwc_status status=resolve_port(s,pid,handle,root,window,&win);
    if(status!=NWC_OK)return status;
    struct nwc_port *p=mut_port(s,win);
    if(!p)return NWC_MISSING;
    if(!authentic(p,pid,handle,root)||!selected(s,p))return NWC_DENIED;
    const struct ws_window *v=ws_find(s->windows,win);
    if(!p->frame.count||!nwc_frame_valid(p,v))return NWC_NOT_READY;
    if(p->frame.generation!=expected)return NWC_STALE;
    struct nwc_patch next={.x=x,.y=y,.w=w,.h=h};
    if(!patch_shape(&next,(unsigned)v->bounds.width,(unsigned)v->bounds.height))return NWC_INVALID;
    for(unsigned i=0;i<w*h;++i){
        if(pixels[i]>0xffffffu)return NWC_INVALID;
        next.pixel[i]=pixels[i];
    }
    unsigned slot=NWC_PATCH_SLOTS,free_slot=NWC_PATCH_SLOTS;
    for(unsigned i=0;i<NWC_PATCH_SLOTS;++i){
        const struct nwc_patch *q=&p->patches[i];
        if(q->w&&q->x==x&&q->y==y){slot=i;break;}
        if(!q->w&&free_slot==NWC_PATCH_SLOTS)free_slot=i;
    }
    if(slot<NWC_PATCH_SLOTS){
        bool equal=true;
        for(unsigned i=0;i<w*h;++i)if(p->patches[slot].pixel[i]!=next.pixel[i]){equal=false;break;}
        if(equal)return NWC_OK;
    }else slot=free_slot;
    if(slot==NWC_PATCH_SLOTS)return NWC_FULL;
    if(expected==UINT32_MAX)return NWC_OVERFLOW;
    next.generation=expected+1;
    p->patches[slot]=next;p->patch_latest=slot+1;++p->frame.generation;
    return NWC_OK;
}
enum nwc_status nwc_frame_generation(struct nwc_service *s,uint32_t pid,uint64_t handle,
    uint64_t root,uint64_t window,uint64_t *generation)
{
    if(!generation)return NWC_INVALID;
    uint32_t win=0;
    enum nwc_status status=resolve_port(s,pid,handle,root,window,&win);
    if(status!=NWC_OK)return status;
    const struct nwc_port *p=find_port(s,win);
    if(!p)return NWC_MISSING;
    if(!authentic(p,pid,handle,root)||!selected(s,p))return NWC_DENIED;
    if(!p->frame.count||!nwc_frame_valid(p,ws_find(s->windows,win)))return NWC_NOT_READY;
    *generation=p->frame.generation;
    return NWC_OK;
}
static uint32_t patch_u32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
enum nwc_status nwc_patch_wire(struct nwc_service *s,uint32_t pid,uint64_t handle,
    uint64_t root,uint64_t window,const uint8_t *wire,size_t bytes,uint64_t *generation)
{
    if(!generation||!wire||bytes!=NWC_PATCH_BYTES||patch_u32(wire)!=1)return NWC_INVALID;
    uint32_t expected=patch_u32(wire+4),x=patch_u32(wire+8),y=patch_u32(wire+12),
        w=patch_u32(wire+16),h=patch_u32(wire+20);
    if(!expected||!w||!h||w>16||h>16)return NWC_INVALID;
    uint32_t pixels[NWC_TILE_PIXELS];
    for(unsigned i=0;i<NWC_TILE_PIXELS;++i){
        pixels[i]=patch_u32(wire+24+4*i);
        if(i<w*h?pixels[i]>0xffffffu:pixels[i]!=0)return NWC_INVALID;
    }
    enum nwc_status result=nwc_publish_patch(s,pid,handle,root,window,expected,x,y,pixels,w,h);
    if(result!=NWC_OK)return result;
    /* Serialized caller: no authority or generation change can interleave. */
    return nwc_frame_generation(s,pid,handle,root,window,generation);
}
const struct nwc_patch *nwc_visible_patches(const struct nwc_service *s,uint32_t win,unsigned *latest)
{
    if(!latest||!nwc_visible_frame(s,win))return 0;
    const struct nwc_port *p=find_port(s,win);
    *latest=p->patch_latest;
    return p->patches;
}
uint32_t nwc_visible_patch_base(const struct nwc_service *s,uint32_t win)
{
    return nwc_visible_frame(s,win)?find_port(s,win)->patch_base:0;
}
enum nwc_status nwc_publish_text(struct nwc_service *s,uint32_t pid,uint64_t handle,
    uint64_t root,uint64_t window,uint32_t expected,const struct nwc_text_run *runs,unsigned count)
{
    if(!expected||count>NWC_TEXT_RUNS||(count&&!runs))return NWC_INVALID;
    uint32_t win=0;
    enum nwc_status status=resolve_port(s,pid,handle,root,window,&win);
    if(status!=NWC_OK)return status;
    struct nwc_port *p=mut_port(s,win);
    if(!p)return NWC_MISSING;
    if(!authentic(p,pid,handle,root)||!selected(s,p))return NWC_DENIED;
    const struct ws_window *w=ws_find(s->windows,win);
    if(!p->frame.count||!nwc_frame_valid(p,w)||p->patch_latest)return NWC_NOT_READY;
    if(p->frame.generation!=expected)return NWC_STALE;
    for(unsigned i=0;i<count;++i)
        if(!text_run_valid(&runs[i],(unsigned)w->bounds.width,(unsigned)w->bounds.height))return NWC_INVALID;
    /* Staged caller-owned immutable runs may not alias this service. */
    if(count){
        uintptr_t a=(uintptr_t)runs,b=(uintptr_t)s;
        size_t bytes=(size_t)count*sizeof *runs;
        if(bytes>UINTPTR_MAX-a||sizeof *s>UINTPTR_MAX-b||
           !(a+bytes<=b||b+sizeof *s<=a))return NWC_INVALID;
    }
    bool equal=count==p->text_count;
    for(unsigned i=0;equal&&i<count;++i){
        const unsigned char *a=(const void *)&runs[i],*b=(const void *)&p->text[i];
        for(size_t j=0;j<sizeof *runs;++j)if(a[j]!=b[j]){equal=false;break;}
    }
    if(equal)return NWC_OK;
    if(expected==UINT32_MAX)return NWC_OVERFLOW;
    zero(p->text,sizeof p->text);
    for(unsigned i=0;i<count;++i)p->text[i]=runs[i];
    p->text_count=count;
    ++p->frame.generation;p->patch_base=p->frame.generation;
    return NWC_OK;
}
enum nwc_status nwc_text_wire(struct nwc_service *s,uint32_t pid,uint64_t handle,
    uint64_t root,uint64_t window,const uint8_t *wire,size_t bytes,uint64_t *generation)
{
    if(!generation||!wire||bytes<NWC_TEXT_HEADER||bytes>NWC_TEXT_WIRE_MAX||
       patch_u32(wire)!=1)return NWC_INVALID;
    uint32_t expected=patch_u32(wire+4),count=patch_u32(wire+8);
    if(!expected||count>NWC_TEXT_RUNS||bytes!=NWC_TEXT_HEADER+count*NWC_TEXT_RUN_BYTES)
        return NWC_INVALID;
    struct nwc_text_run runs[NWC_TEXT_RUNS]={0};
    for(unsigned i=0;i<count;++i){
        const uint8_t *p=wire+NWC_TEXT_HEADER+i*NWC_TEXT_RUN_BYTES;
        struct nwc_text_run *q=&runs[i];
        q->x=patch_u32(p);q->y=patch_u32(p+4);q->width=patch_u32(p+8);
        q->foreground=patch_u32(p+12);q->background=patch_u32(p+16);q->count=patch_u32(p+20);
        for(unsigned j=0;j<NWC_TEXT_GLYPHS;++j)q->cp[j]=patch_u32(p+24+4*j);
    }
    enum nwc_status result=nwc_publish_text(s,pid,handle,root,window,expected,runs,count);
    if(result!=NWC_OK)return result;
    return nwc_frame_generation(s,pid,handle,root,window,generation);
}
const struct nwc_text_run *nwc_visible_text(const struct nwc_service *s,uint32_t win,unsigned *count)
{
    if(!count||!nwc_visible_frame(s,win))return 0;
    const struct nwc_port *p=find_port(s,win);
    *count=p->text_count;
    return p->text;
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
