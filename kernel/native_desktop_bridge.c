#include <styleos/native_desktop_bridge.h>

/* The maximum tile is 16x16. Window coordinates are trusted registry fields
 * but may legitimately be negative; arithmetic stays within signed int. */
static struct desk_box clipped(const struct ws_bounds *bounds,
                               const struct nwc_tile *tile,int width,int height)
{
    struct desk_box out={0};
    if(!bounds || !tile || width<=0 || height<=0 ||
       bounds->width<=0 || bounds->height<=0 ||
       !tile->width || !tile->height ||
       tile->width>NWC_TILE_WIDTH || tile->height>NWC_TILE_HEIGHT)return out;
    int left=bounds->x>0?bounds->x:0;
    int top=bounds->y>0?bounds->y:0;
    int right=bounds->x+(bounds->width<(int)tile->width?bounds->width:(int)tile->width);
    int bottom=bounds->y+(bounds->height<(int)tile->height?bounds->height:(int)tile->height);
    if(right>width)right=width;
    if(bottom>height)bottom=height;
    if(left>=right || top>=bottom)return out;
    out.x=left;out.y=top;out.w=right-left;out.h=bottom-top;
    return out;
}
static struct desk_box frame_clip(const struct ws_bounds *b,const struct nwc_rect *q,int width,int height)
{
    struct desk_box out={0};
    int x=b->x+(int)q->x,y=b->y+(int)q->y;
    int right=x+(int)q->w,bottom=y+(int)q->h;
    if(x<0)x=0;
    if(y<0)y=0;
    if(right>width)right=width;
    if(bottom>height)bottom=height;
    if(right>x&&bottom>y)out=(struct desk_box){x,y,right-x,bottom-y};
    return out;
}
static bool same(struct ndb_seen a,struct ndb_seen b)
{
    return a.window_id==b.window_id && a.generation==b.generation &&
           desk_box_equal(a.rect,b.rect);
}
static unsigned snapshot(const struct nwc_service *ipc,int width,int height,
                         struct ndb_seen out[NWC_PORTS])
{
    if(!ipc || !ipc->ready || !ipc->windows || width<=0 || height<=0)return 0;
    unsigned count=0;
    for(unsigned i=0;i<WS_MAX_WINDOWS && count<NWC_PORTS;++i){
        const struct ws_window *window=&ipc->windows->windows[i];
        if(!window->id || window->role!=WS_APP_WINDOW)continue;
        const struct nwc_frame *frame=nwc_visible_frame(ipc,window->id);
        struct desk_box rect;
        uint32_t generation;
        if(frame){
            rect=frame_clip(&window->bounds,&frame->rect[0],width,height);
            generation=frame->generation;
        }else{
            struct nwc_tile tile;
            if(nwc_visible_tile(ipc,window->id,&tile)!=NWC_OK)continue;
            rect=clipped(&window->bounds,&tile,width,height);
            generation=tile.generation;
        }
        if(!rect.w)continue;
        out[count++]=(struct ndb_seen){window->id,generation,rect};
    }
    return count;
}
/* Stable default registry order, with one explicitly raised visible surface
 * at the end. No registry slots are moved and no user buffer is changed. */
static unsigned ordered(const struct nwc_service *ipc,int width,int height,uint32_t top,
                        struct ndb_seen out[NWC_PORTS])
{
    unsigned count=snapshot(ipc,width,height,out);
    for(unsigned i=0;i<count;++i)if(out[i].window_id==top){
        struct ndb_seen raised=out[i];
        for(unsigned j=i;j+1<count;++j)out[j]=out[j+1];
        out[count-1]=raised;break;
    }
    return count;
}
unsigned ndb_track_top(const struct nwc_service *ipc,struct ndb_tracker *tracker,
                       int width,int height,uint32_t top,struct desk_damage *damage)
{
    if(!tracker || !damage || tracker->count>NWC_PORTS)return 0;
    struct ndb_seen next[NWC_PORTS]={0};
    unsigned count=ordered(ipc,width,height,top,next), changes=0;
    for(unsigned i=0;i<tracker->count;++i){
        struct ndb_seen previous=tracker->last[i];
        bool unchanged=false;
        for(unsigned j=0;j<count;++j)
            if(previous.window_id==next[j].window_id && same(previous,next[j])){
                unchanged=true;break;
            }
        if(!unchanged){desk_damage_add(damage,previous.rect);++changes;}
    }
    for(unsigned i=0;i<count;++i){
        bool unchanged=false;
        for(unsigned j=0;j<tracker->count;++j)
            if(next[i].window_id==tracker->last[j].window_id &&
               same(next[i],tracker->last[j])){unchanged=true;break;}
        if(!unchanged){desk_damage_add(damage,next[i].rect);++changes;}
    }
    /* A relative order reversal changes overlapping pixels even when both
     * content generations and rectangles are identical. Repaint both
     * bounded regions; additions/removals keep original damage semantics. */
    for(unsigned i=0;i<tracker->count;++i)for(unsigned j=i+1;j<tracker->count;++j){
        unsigned ni=count,nj=count;
        for(unsigned k=0;k<count;++k){
            if(next[k].window_id==tracker->last[i].window_id)ni=k;
            if(next[k].window_id==tracker->last[j].window_id)nj=k;
        }
        if(ni<count&&nj<count&&ni>nj){
            desk_damage_add(damage,next[ni].rect);desk_damage_add(damage,next[nj].rect);
            changes+=2;
        }
    }
    for(unsigned i=0;i<NWC_PORTS;++i)
        tracker->last[i]=i<count?next[i]:(struct ndb_seen){0};
    tracker->count=count;
    return changes;
}
unsigned ndb_track(const struct nwc_service *ipc,struct ndb_tracker *tracker,
                   int width,int height,struct desk_damage *damage)
{
    return ndb_track_top(ipc,tracker,width,height,0,damage);
}
unsigned ndb_paint_top(const struct nwc_service *ipc,int width,int height,uint32_t top,
                       ndb_pixel_plot plot,ndb_rect_plot fill,void *opaque)
{
    if(!ipc || !ipc->ready || !ipc->windows || !plot || width<=0 || height<=0)return 0;
    unsigned painted=0;
    unsigned raised=WS_MAX_WINDOWS;
    for(unsigned i=0;i<WS_MAX_WINDOWS;++i)
        if(top&&ipc->windows->windows[i].id==top){raised=i;break;}
    for(unsigned i=0;i<=WS_MAX_WINDOWS;++i){
        if(i==raised&&i<WS_MAX_WINDOWS)continue;
        if(i==WS_MAX_WINDOWS&&raised==WS_MAX_WINDOWS)break;
        const struct ws_window *window=&ipc->windows->windows[i==WS_MAX_WINDOWS?raised:i];
        if(!window->id || window->role!=WS_APP_WINDOW)continue;
        const struct nwc_frame *frame=nwc_visible_frame(ipc,window->id);
        if(frame){
            for(unsigned j=0;j<frame->count;++j){
                struct desk_box r=frame_clip(&window->bounds,&frame->rect[j],width,height);
                if(!r.w)continue;
                if(fill)fill(opaque,r.x,r.y,r.w,r.h,frame->rect[j].rgb);
                else for(int y=r.y;y<r.y+r.h;++y)
                    for(int x=r.x;x<r.x+r.w;++x)plot(opaque,x,y,frame->rect[j].rgb);
                unsigned n=(unsigned)r.w*(unsigned)r.h;
                painted=n>UINT32_MAX-painted?UINT32_MAX:painted+n;
            }
            continue;
        }
        struct nwc_tile tile;
        if(nwc_visible_tile(ipc,window->id,&tile)!=NWC_OK)continue;
        struct desk_box r=clipped(&window->bounds,&tile,width,height);
        for(int y=r.y;y<r.y+r.h;++y)
            for(int x=r.x;x<r.x+r.w;++x){
                unsigned src=(unsigned)(y-window->bounds.y)*tile.width+
                             (unsigned)(x-window->bounds.x);
                plot(opaque,x,y,tile.pixel[src]&UINT32_C(0x00ffffff));
                if(painted<UINT32_MAX)++painted;
            }
    }
    return painted;
}

unsigned ndb_paint_rects(const struct nwc_service *ipc,int width,int height,
                         ndb_pixel_plot plot,ndb_rect_plot fill,void *opaque)
{
    return ndb_paint_top(ipc,width,height,0,plot,fill,opaque);
}
unsigned ndb_paint(const struct nwc_service *ipc,int width,int height,
                   ndb_pixel_plot plot,void *opaque)
{
    return ndb_paint_rects(ipc,width,height,plot,0,opaque);
}
/* Reuse snapshot instead of a second, inconsistent definition of the
 * visible region. Later painted entries are in front. */
uint32_t ndb_hit_top(const struct nwc_service *ipc,int width,int height,uint32_t top,int x,int y)
{
    struct ndb_seen seen[NWC_PORTS]={0};
    unsigned n=ordered(ipc,width,height,top,seen);
    while(n){
        const struct ndb_seen *v=&seen[--n];
        if(x>=v->rect.x&&y>=v->rect.y&&(int64_t)x-v->rect.x<v->rect.w&&(int64_t)y-v->rect.y<v->rect.h)
            return v->window_id;
    }
    return 0;
}
uint32_t ndb_hit(const struct nwc_service *ipc,int width,int height,int x,int y)
{
    return ndb_hit_top(ipc,width,height,0,x,y);
}
static bool focus_visible(const struct nwc_service *ipc,int width,int height,uint32_t id)
{
    struct ndb_seen seen[NWC_PORTS]={0};
    unsigned n=snapshot(ipc,width,height,seen);
    for(unsigned i=0;i<n;++i)if(seen[i].window_id==id)return true;
    return false;
}
bool ndb_route_pointer(struct nwc_service *ipc,struct ndb_input *input,int width,int height,
                        int x,int y,uint32_t buttons,uint32_t modifiers,
                        bool pressed,bool released,bool blocked)
{
    if(!input)return false;
    if(pressed){
        input->focus=blocked?0:ndb_hit_top(ipc,width,height,input->raised,x,y);
        input->capture=input->focus;
        if(input->focus)input->raised=input->focus;
    }
    uint32_t id=input->capture;
    if(!id)return false;
    if(!blocked&&focus_visible(ipc,width,height,id)){
        const struct ws_window *w=ws_find(ipc->windows,id);
        if(w){
            int64_t rx=(int64_t)x-w->bounds.x,ry=(int64_t)y-w->bounds.y;
            if(rx>=INT32_MIN&&rx<=INT32_MAX&&ry>=INT32_MIN&&ry<=INT32_MAX)
                (void)nwc_send_pointer(ipc,id,(int32_t)rx,(int32_t)ry,buttons,modifiers);
        }
    }else if(input->focus==id)input->focus=0;
    if(released)input->capture=0;
    return true;
}
bool ndb_route_key(struct nwc_service *ipc,struct ndb_input *input,int width,int height,
                    uint32_t key,uint32_t modifiers,bool blocked)
{
    if(!input)return false;
    if(blocked){input->focus=0;return false;}
    uint32_t id=input->focus;
    if(!id)return false;
    if(!focus_visible(ipc,width,height,id))input->focus=0;
    else (void)nwc_send_key(ipc,id,key,modifiers,true);
    /* A full queue or revoked port must not reroute this key to a legacy app. */
    return true;
}

enum nwc_status ndb_request_close(struct nwc_service *ipc,const struct ndb_input *input,
                                  int width,int height,uint32_t expected_window)
{
    if(!ipc||!input||!expected_window)return NWC_INVALID;
    if(input->focus!=expected_window||input->capture||
       !focus_visible(ipc,width,height,expected_window))return NWC_DENIED;
    return nwc_send_key(ipc,expected_window,0x3eu,DESK_ALT,true);
}

bool ndb_cycle_focus(const struct nwc_service *ipc,struct ndb_input *input,int width,int height)
{
    if(!input||input->capture)return false;
    struct ndb_seen seen[NWC_PORTS]={0};
    unsigned count=snapshot(ipc,width,height,seen);
    if(!count){input->focus=0;input->raised=0;return false;}
    for(unsigned i=0;i<count;++i)if(seen[i].window_id==input->focus){
        if(count==1)return false;
        input->focus=seen[(i+count-1)%count].window_id;
        input->raised=input->focus;
        return true;
    }
    input->focus=seen[count-1].window_id;
    input->raised=input->focus;
    return true;
}

/* Panel coordinates intentionally preserve the previous five-slot layout. */
struct desk_box ndb_task_box(int width,int height,unsigned slots,unsigned index)
{
    if(width<800||height<600||slots<NDB_LEGACY_TASKS||
       slots>NDB_LEGACY_TASKS+NWC_PORTS||index>=slots)return (struct desk_box){0};
    int step=(width-252)/(int)slots;
    if(step>160)step=160;
    return (struct desk_box){164+(int)index*step,height-52,step-4,48};
}
bool ndb_task_equal(const struct nwc_task *a,const struct nwc_task *b)
{
    return a&&b&&a->window_id&&a->window_id==b->window_id&&a->pid==b->pid&&
           a->session_id==b->session_id&&a->generation==b->generation&&
           a->root==b->root&&a->minimized==b->minimized;
}
bool ndb_task_panel_update(const struct nwc_service *ipc,struct ndb_task_panel *panel,
                           int width,int height,unsigned legacy,uint32_t focus,
                           struct desk_damage *damage)
{
    if(!panel||!damage||legacy>NDB_LEGACY_TASKS||width<800||height<600||
       panel->count>NWC_PORTS)return false;
    struct ndb_task_panel next={0};
    next.legacy=legacy;
    size_t n=nwc_task_snapshot(ipc,next.tasks,NWC_PORTS);
    if(n>NWC_PORTS)return false;
    next.count=(unsigned)n;next.slots=legacy+next.count;
    if(next.slots<NDB_LEGACY_TASKS)next.slots=NDB_LEGACY_TASKS;
    for(unsigned i=0;i<next.count;++i)
        if(next.tasks[i].window_id==focus&&!next.tasks[i].minimized)next.focus=focus;
    bool changed=false;
    if((panel->count||next.count)&&(panel->legacy!=legacy||panel->slots!=next.slots)){
        desk_damage_add(damage,(struct desk_box){164,height-52,width-266,48});
        changed=true;
    }else{
        for(unsigned i=0;i<panel->count||i<next.count;++i){
            if(i<panel->count&&i<next.count&&ndb_task_equal(&panel->tasks[i],&next.tasks[i])&&
               (panel->focus==panel->tasks[i].window_id)==(next.focus==next.tasks[i].window_id))continue;
            desk_damage_add(damage,ndb_task_box(width,height,next.slots,legacy+i));
            changed=true;
        }
    }
    *panel=next;
    return changed;
}
const struct nwc_task *ndb_task_hit(const struct ndb_task_panel *panel,int width,int height,
                                    unsigned legacy,int x,int y)
{
    if(!panel||panel->legacy!=legacy||panel->count>NWC_PORTS)return 0;
    for(unsigned i=0;i<panel->count;++i){
        struct desk_box b=ndb_task_box(width,height,panel->slots,legacy+i);
        if(b.w>0&&x>=b.x&&y>=b.y&&(int64_t)x-b.x<b.w&&(int64_t)y-b.y<b.h)
            return &panel->tasks[i];
    }
    return 0;
}

bool ndb_task_get(const struct nwc_service *ipc,uint32_t window,struct nwc_task *out)
{
    if(!window||!out)return false;
    struct nwc_task tasks[NWC_PORTS];
    size_t n=nwc_task_snapshot(ipc,tasks,NWC_PORTS);
    if(n>NWC_PORTS)return false;
    for(size_t i=0;i<n;++i)if(tasks[i].window_id==window){*out=tasks[i];return true;}
    return false;
}
bool ndb_task_current(const struct nwc_service *ipc,const struct nwc_task *token)
{
    struct nwc_task current;
    return token&&ndb_task_get(ipc,token->window_id,&current)&&ndb_task_equal(token,&current);
}
struct desk_box ndb_task_hover_box(const struct ndb_task_panel *panel,int width,int height,
                                    const struct nwc_task *token)
{
    if(!panel||panel->count>NWC_PORTS||!token||!token->window_id)return (struct desk_box){0};
    for(unsigned i=0;i<panel->count;++i)
        if(ndb_task_equal(&panel->tasks[i],token))
            return ndb_task_box(width,height,panel->slots,panel->legacy+i);
    return (struct desk_box){0};
}
