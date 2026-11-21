#include <styleos/native_window_channel.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct pr_table processes;
static struct ws_service windows;
static struct usr_runtime users;
static struct nwc_service ipc;
static struct nwc_event event;
static struct nwc_tile tile;
static uint32_t pixels[NWC_TILE_PIXELS];

static uint32_t admit(uint64_t handle,const char *label)
{
    struct pr_admission a={PR_NATIVE_USER,0,handle,handle*4096u,0,label};
    uint32_t pid=0;
    assert(pr_admit(&processes,&a,&pid)==PR_OK);
    return pid;
}
int main(void)
{
    pr_init(&processes);ws_init(&windows);
    assert(nwc_global()==0);
    assert(nwc_start(&ipc,&processes,&users,&windows)==NWC_NOT_READY);
    assert(usr_init(&users,&processes,&windows)==USR_OK);
    assert(nwc_start(&ipc,&processes,&users,&windows)==NWC_OK);
    assert(nwc_start(&ipc,&processes,&users,&windows)==NWC_STALE);
    assert(nwc_port_count(&ipc)==0);
    uint32_t sid_a=0,sid_b=0;
    assert(usr_create(&users,111,&sid_a)==USR_OK);
    assert(usr_create(&users,222,&sid_b)==USR_OK);
    assert(usr_select(&users,sid_a)==USR_OK);
    uint32_t pid_a=admit(16,"Editor.tea"),pid_b=admit(32,"Documents.tea");
    assert(usr_bind(&users,sid_a,pid_a,16)==USR_OK);
    const struct ws_bounds bounds={12,20,500,300};
    uint32_t win_a=0,win_b=0;
    assert(ws_open_app(&windows,&processes,pid_a,"Editor",bounds,&win_a)==WS_OK);
    assert(nwc_claim(&ipc,pid_a,17,65536,win_a)==NWC_DENIED);
    assert(nwc_claim(&ipc,pid_b,32,131072,win_a)==NWC_DENIED);
    assert(nwc_claim(&ipc,pid_a,16,65536,win_a)==NWC_OK);
    assert(nwc_claim(&ipc,pid_a,16,65536,win_a)==NWC_STALE);
    assert(nwc_port_count(&ipc)==1);
    assert(nwc_receive(&ipc,pid_a,16,65536,win_a,&event)==NWC_EMPTY);
    assert(nwc_send_key(&ipc,win_a,0,0,true)==NWC_INVALID);
    assert(nwc_send_key(&ipc,win_a,'A',0,true)==NWC_OK);
    const uint8_t txt[]={'H','e','l','l','o'};
    assert(nwc_send_text(&ipc,win_a,txt,sizeof(txt))==NWC_OK);
    assert(nwc_send_key(&ipc,win_a,'B',1,false)==NWC_OK);
    assert(nwc_send_key(&ipc,win_a,'C',0,true)==NWC_OK);
    assert(nwc_send_key(&ipc,win_a,'D',0,true)==NWC_FULL);
    assert(nwc_send_text(&ipc,win_a,txt,33)==NWC_INVALID);
    memset(&event,0x5a,sizeof(event));
    assert(nwc_receive(&ipc,pid_a,99,65536,win_a,&event)==NWC_DENIED);
    assert(event.sequence==0x5a5a5a5a);
    assert(nwc_receive(&ipc,pid_a,16,65536,win_a,&event)==NWC_OK);
    assert(event.sequence==1 && event.kind==NWC_KEY && event.code=='A' && event.down);
    assert(nwc_receive(&ipc,pid_a,16,65536,win_a,&event)==NWC_OK);
    assert(event.sequence==2 && event.kind==NWC_TEXT && event.size==5 &&
           !memcmp(event.bytes,txt,5) && event.bytes[5]==0);
    assert(nwc_receive(&ipc,pid_a,16,65536,win_a,&event)==NWC_OK);
    assert(event.sequence==3 && event.code=='B' && !event.down);
    assert(nwc_receive(&ipc,pid_a,16,65536,win_a,&event)==NWC_OK);
    assert(event.sequence==4 && event.code=='C');
    assert(nwc_receive(&ipc,pid_a,16,65536,win_a,&event)==NWC_EMPTY);

    for(unsigned i=0;i<NWC_TILE_PIXELS;++i)pixels[i]=0xff001100u+i;
    assert(nwc_publish_tile(&ipc,pid_a,16,65536,win_a,pixels,0,16)==NWC_INVALID);
    assert(nwc_publish_tile(&ipc,pid_a,16,65536,win_a,pixels,17,16)==NWC_INVALID);
    assert(nwc_publish_tile(&ipc,pid_a,16,65536,win_a,pixels,16,16)==NWC_OK);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK);
    assert(tile.width==16 && tile.height==16 && tile.generation==1 &&
           tile.pixel[0]==0x1100 && tile.pixel[255]==(0x1100+255));
    assert(nwc_publish_tile(&ipc,pid_a,16,65536,win_a,pixels,1,1)==NWC_OK);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK);
    assert(tile.generation==2 && tile.width==1 && tile.height==1 && tile.pixel[1]==0);
    assert(ws_compositor_restart(&windows)==2);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK && tile.generation==2);
    assert(ws_set_minimized(&windows,pid_a,win_a,true)==WS_OK);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_DENIED);
    assert(nwc_send_key(&ipc,win_a,'X',0,true)==NWC_DENIED);
    assert(ws_set_minimized(&windows,pid_a,win_a,false)==WS_OK);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK);

    assert(usr_select(&users,sid_b)==USR_OK);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_DENIED);
    assert(nwc_send_key(&ipc,win_a,'Q',0,true)==NWC_DENIED);
    memset(&event,0x5a,sizeof(event));
    assert(nwc_receive(&ipc,pid_a,16,65536,win_a,&event)==NWC_DENIED &&
           event.sequence==0x5a5a5a5a);
    assert(usr_bind(&users,sid_b,pid_b,32)==USR_OK);
    assert(ws_open_app(&windows,&processes,pid_b,"Documents",bounds,&win_b)==WS_OK);
    assert(nwc_claim(&ipc,pid_b,32,131072,win_b)==NWC_OK);
    assert(nwc_send_key(&ipc,win_b,'Z',0,true)==NWC_OK);
    assert(nwc_receive(&ipc,pid_a,16,65536,win_b,&event)==NWC_DENIED);
    assert(nwc_receive(&ipc,pid_b,32,131072,win_b,&event)==NWC_OK);
    assert(event.sequence==1 && event.code=='Z' && event.window_id==win_b);
    assert(nwc_port_count(&ipc)==2);
    assert(usr_lock(&users)==USR_OK);
    assert(nwc_send_key(&ipc,win_b,'Z',0,true)==NWC_DENIED);
    assert(nwc_visible_tile(&ipc,win_b,&tile)==NWC_DENIED);
    assert(usr_select(&users,sid_a)==USR_OK);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK);
    assert(nwc_visible_tile(&ipc,win_b,&tile)==NWC_DENIED);
    assert(nwc_publish_tile(&ipc,pid_b,32,131072,win_b,pixels,1,1)==NWC_DENIED);
    /* Closing a window immediately revokes its IPC; port will be zeroed when
     * the trusted compositor calls reconcile. */
    assert(ws_close_app(&windows,pid_a,win_a)==WS_OK);
    assert(nwc_send_key(&ipc,win_a,'X',0,true)==NWC_DENIED);
    assert(nwc_reconcile(&ipc)==1 && nwc_port_count(&ipc)==1);
    assert(nwc_claim(&ipc,pid_a,16,65536,win_a)==NWC_DENIED);
    assert(pr_transition(&processes,pid_b,32,PR_FAULTED)==PR_OK);
    assert(usr_reconcile(&users)==1);
    assert(nwc_reconcile(&ipc)==1 && nwc_port_count(&ipc)==0);
    assert(nwc_receive(&ipc,pid_b,32,131072,win_b,&event)==NWC_MISSING);
    puts("NATIVE WINDOW CHANNEL HOST PASS: real PID, selected sessions, copied input/tile, overflow, restart, fault and cleanup");
    return 0;
}
