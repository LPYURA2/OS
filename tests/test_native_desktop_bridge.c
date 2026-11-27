#include <styleos/native_desktop_bridge.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct pr_table processes;
static struct ws_service windows;
static struct usr_runtime sessions;
static struct nwc_service ipc;
static struct ndb_tracker tracked;
static uint32_t framebuffer[800u*600u];
static struct desktop_surface surface={
    (uintptr_t)framebuffer,sizeof(framebuffer),800,600,800,DESKTOP_BGR_BYTES
};
static uint32_t pattern[NWC_TILE_PIXELS];
static void pixel(void *opaque,int x,int y,uint32_t color)
{
    unsigned *calls=(unsigned *)opaque;
    if(calls)++*calls;
    desk_fill(x,y,1,1,color);
}
static void scene(void)
{
    desk_fill(0,0,800,600,0x142030);
    (void)ndb_paint(&ipc,800,540,pixel,0);
}
static void update(unsigned expect)
{
    struct desk_damage d={0};
    unsigned n=ndb_track(&ipc,&tracked,800,540,&d);
    assert(n==expect);
    if(n)assert(d.all || d.count);
    if(n)assert(desk_present(&d,scene)!=0);
}
static uint32_t create(uint64_t handle,const char *name)
{
    uint32_t pid=0;
    struct pr_admission a={PR_NATIVE_USER,0,handle,handle*4096u,0,name};
    assert(pr_admit(&processes,&a,&pid)==PR_OK);
    return pid;
}
int main(void)
{
    pr_init(&processes);ws_init(&windows);
    assert(usr_init(&sessions,&processes,&windows)==USR_OK);
    assert(nwc_start(&ipc,&processes,&sessions,&windows)==NWC_OK);
    desk_draw_init(&surface);
    update(0);
    assert(nwc_port_count(&ipc)==0);
    assert(ndb_paint(&ipc,800,540,pixel,0)==0);
    uint32_t s1=0,s2=0;
    assert(usr_create(&sessions,111,&s1)==USR_OK);
    assert(usr_create(&sessions,222,&s2)==USR_OK);
    assert(usr_select(&sessions,s1)==USR_OK);
    uint32_t p1=create(16,"First.tea"),p2=create(32,"Second.tea");
    assert(usr_bind(&sessions,s1,p1,16)==USR_OK);
    uint32_t w1=0,w2=0;
    assert(ws_open_app(&windows,&processes,p1,"First",
           (struct ws_bounds){796,538,80,60},&w1)==WS_OK);
    assert(nwc_claim(&ipc,p1,16,65536,w1)==NWC_OK);
    for(unsigned i=0;i<NWC_TILE_PIXELS;++i)pattern[i]=0xff446688;
    assert(nwc_publish_tile(&ipc,p1,16,65536,w1,pattern,16,16)==NWC_OK);
    update(1);
    assert(framebuffer[538u*800u+796u]==0x446688);
    assert(framebuffer[539u*800u+799u]==0x446688);
    assert(framebuffer[540u*800u+799u]==0);
    update(0);
    struct ndb_input input={0};
    struct nwc_event ev={0};
    assert(ndb_hit(&ipc,800,540,799,539)==w1);
    assert(!ndb_hit(&ipc,800,540,800,539));
    assert(!ndb_hit(&ipc,800,540,799,540));
    assert(!ndb_route_pointer(&ipc,&input,800,540,799,539,1,0,true,false,true));
    assert(!input.focus&&!input.capture);
    assert(ndb_route_pointer(&ipc,&input,800,540,799,539,1,0,true,false,false));
    assert(input.focus==w1&&input.capture==w1);
    assert(ndb_route_pointer(&ipc,&input,800,540,790,530,0,0,false,true,false));
    assert(input.focus==w1&&!input.capture);
    assert(ndb_route_key(&ipc,&input,800,540,0x1e,1,false));
    assert(nwc_receive(&ipc,p1,16,65536,w1,&ev)==NWC_OK&&ev.kind==NWC_POINTER&&ev.down&&ev.size==8&&ev.bytes[0]==3&&ev.bytes[4]==1);
    assert(nwc_receive(&ipc,p1,16,65536,w1,&ev)==NWC_OK&&ev.kind==NWC_POINTER&&!ev.down&&ev.bytes[0]==250&&ev.bytes[3]==255&&ev.bytes[4]==248);
    assert(nwc_receive(&ipc,p1,16,65536,w1,&ev)==NWC_OK&&ev.kind==NWC_KEY&&ev.code==0x1e&&ev.modifiers==1);
    assert(nwc_receive(&ipc,p1,16,65536,w1,&ev)==NWC_EMPTY);
    assert(ndb_route_key(&ipc,&input,800,540,0x30,0,false));
    assert(ndb_route_key(&ipc,&input,800,540,0x30,0,false));
    assert(ndb_route_key(&ipc,&input,800,540,0x30,0,false));
    assert(ndb_route_key(&ipc,&input,800,540,0x30,0,false));
    assert(ndb_route_key(&ipc,&input,800,540,0x2e,0,false)); /* full, still consumed */
    for(unsigned n=0;n<NWC_EVENTS;++n)
        assert(nwc_receive(&ipc,p1,16,65536,w1,&ev)==NWC_OK&&ev.code==0x30);
    assert(nwc_receive(&ipc,p1,16,65536,w1,&ev)==NWC_EMPTY);
    assert(!ndb_route_pointer(&ipc,&input,800,540,700,500,1,0,true,false,false));
    assert(!input.focus&&!input.capture);
    assert(!ndb_route_key(&ipc,&input,800,540,0x1e,0,false));
    /* Wrong owner cannot replace a displayed native tile. */
    pattern[0]=0x112233;
    assert(nwc_publish_tile(&ipc,p1,999,65536,w1,pattern,16,16)==NWC_DENIED);
    update(0);
    assert(framebuffer[538u*800u+796u]==0x446688);
    /* A correct update invalidates only the old/new visible region. */
    assert(nwc_publish_tile(&ipc,p1,16,65536,w1,pattern,16,16)==NWC_OK);
    update(2);
    assert(framebuffer[538u*800u+796u]==0x112233);
    /* Window compositor restart must preserve a live process's image. */
    assert(ws_compositor_restart(&windows)==2);
    update(0);
    assert(framebuffer[538u*800u+796u]==0x112233);
    assert(ws_set_minimized(&windows,p1,w1,true)==WS_OK);
    update(1);
    assert(framebuffer[538u*800u+796u]==0x142030);
    assert(ws_set_minimized(&windows,p1,w1,false)==WS_OK);
    update(1);
    assert(framebuffer[538u*800u+796u]==0x112233);
    /* New user cannot view the previous user's pixels. */
    assert(usr_select(&sessions,s2)==USR_OK);
    update(1);
    assert(framebuffer[538u*800u+796u]==0x142030);
    assert(usr_bind(&sessions,s2,p2,32)==USR_OK);
    assert(ws_open_app(&windows,&processes,p2,"Second",
           (struct ws_bounds){10,16,300,120},&w2)==WS_OK);
    assert(nwc_claim(&ipc,p2,32,131072,w2)==NWC_OK);
    for(unsigned i=0;i<NWC_TILE_PIXELS;++i)pattern[i]=0xabcdef;
    assert(nwc_publish_tile(&ipc,p2,32,131072,w2,pattern,16,16)==NWC_OK);
    update(1);
    assert(framebuffer[16u*800u+10u]==0xabcdef);
    const struct nwc_rect frame[]={{0,0,300,120,0x112233},{20,30,160,60,0x8899aa},{25,35,5,5,0x00ff00}};
    const struct nwc_rect invalid[]={{0,0,300,120,0x112233},{299,0,2,1,0}};
    assert(nwc_publish_frame(&ipc,p1,16,65536,w2,frame,3)==NWC_DENIED);
    assert(nwc_publish_frame(&ipc,p2,32,131072,w2,invalid,2)==NWC_INVALID);
    update(0);
    assert(nwc_publish_frame(&ipc,p2,32,131072,w2,frame,3)==NWC_OK);
    const struct nwc_frame *view=nwc_visible_frame(&ipc,w2);
    assert(view&&view->generation==2&&view->count==3);
    update(2);
    assert(framebuffer[16u*800u+10u]==0x112233);
    assert(framebuffer[46u*800u+30u]==0x8899aa);
    assert(framebuffer[51u*800u+35u]==0x00ff00);
    assert(framebuffer[135u*800u+309u]==0x112233);
    assert(framebuffer[136u*800u+310u]==0x142030);
    assert(ndb_hit(&ipc,800,540,309,135)==w2);
    assert(nwc_publish_frame(&ipc,p2,32,131072,w2,invalid,2)==NWC_INVALID);
    update(0);
    assert(ws_compositor_restart(&windows)==3);
    update(0);
    assert(nwc_publish_tile(&ipc,p2,32,131072,w2,pattern,16,16)==NWC_OK);
    assert(!nwc_visible_frame(&ipc,w2));
    struct nwc_tile restored={0};
    assert(nwc_visible_tile(&ipc,w2,&restored)==NWC_OK&&restored.generation==3);
    update(2);
    assert(framebuffer[135u*800u+309u]==0x142030);
    assert(ndb_route_pointer(&ipc,&input,800,540,10,16,1,0,true,false,false));
    assert(input.focus==w2);
    assert(usr_lock(&sessions)==USR_OK);
    assert(ndb_route_pointer(&ipc,&input,800,540,10,16,0,0,false,true,false));
    assert(!input.focus&&!input.capture);
    assert(!ndb_hit(&ipc,800,540,10,16));
    update(1);
    assert(framebuffer[16u*800u+10u]==0x142030);
    assert(usr_select(&sessions,s2)==USR_OK);
    update(1);
    assert(framebuffer[16u*800u+10u]==0xabcdef);
    assert(pr_transition(&processes,p2,32,PR_FAULTED)==PR_OK);
    assert(usr_reconcile(&sessions)==1);
    assert(nwc_reconcile(&ipc)==1);
    update(1);
    assert(framebuffer[16u*800u+10u]==0x142030);
    assert(nwc_port_count(&ipc)==1);
    assert(usr_select(&sessions,s1)==USR_OK);
    update(1);
    assert(framebuffer[538u*800u+796u]==0x112233);
    assert(ws_close_app(&windows,p1,w1)==WS_OK);
    assert(nwc_reconcile(&ipc)==1);
    update(1);
    assert(framebuffer[538u*800u+796u]==0x142030);
    assert(nwc_port_count(&ipc)==0);
    puts("STYLEOS 774 DESKTOP BRIDGE PASS: authentic pixels, clipping, damage, restart, user switch, lock, fault and stale cleanup");
    return 0;
}
