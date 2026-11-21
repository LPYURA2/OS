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
        struct nwc_tile tile;
        if(nwc_visible_tile(ipc,window->id,&tile)!=NWC_OK)continue;
        struct desk_box rect=clipped(&window->bounds,&tile,width,height);
        if(!rect.w)continue;
        out[count++]=(struct ndb_seen){window->id,tile.generation,rect};
    }
    return count;
}
unsigned ndb_track(const struct nwc_service *ipc,struct ndb_tracker *tracker,
                   int width,int height,struct desk_damage *damage)
{
    if(!tracker || !damage || tracker->count>NWC_PORTS)return 0;
    struct ndb_seen next[NWC_PORTS]={0};
    unsigned count=snapshot(ipc,width,height,next), changes=0;
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
    for(unsigned i=0;i<NWC_PORTS;++i)
        tracker->last[i]=i<count?next[i]:(struct ndb_seen){0};
    tracker->count=count;
    return changes;
}
unsigned ndb_paint(const struct nwc_service *ipc,int width,int height,
                   ndb_pixel_plot plot,void *opaque)
{
    if(!ipc || !ipc->ready || !ipc->windows || !plot || width<=0 || height<=0)return 0;
    unsigned painted=0;
    for(unsigned i=0;i<WS_MAX_WINDOWS;++i){
        const struct ws_window *window=&ipc->windows->windows[i];
        if(!window->id || window->role!=WS_APP_WINDOW)continue;
        struct nwc_tile tile;
        if(nwc_visible_tile(ipc,window->id,&tile)!=NWC_OK)continue;
        struct desk_box r=clipped(&window->bounds,&tile,width,height);
        for(int y=r.y;y<r.y+r.h;++y)
            for(int x=r.x;x<r.x+r.w;++x){
                unsigned src=(unsigned)(y-window->bounds.y)*tile.width+
                             (unsigned)(x-window->bounds.x);
                plot(opaque,x,y,tile.pixel[src]&UINT32_C(0x00ffffff));
                ++painted;
            }
    }
    return painted;
}
