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

    uint8_t wire[NWC_INPUT_BYTES];
    memset(wire,0x5a,sizeof wire);
    assert(nwc_receive_wire(&ipc,pid_a,16,65536,0,wire)==NWC_EMPTY && wire[0]==0x5a);
    assert(nwc_send_text(&ipc,win_a,txt,sizeof txt)==NWC_OK);
    assert(nwc_receive_wire(&ipc,pid_b,32,131072,win_a,wire)==NWC_DENIED && wire[0]==0x5a);
    assert(nwc_receive_wire(&ipc,pid_a,16,65536,UINT64_C(0x100000001),wire)==NWC_INVALID && wire[0]==0x5a);
    assert(nwc_receive_wire(&ipc,pid_a,16,65536,0,wire)==NWC_OK);
    assert(wire[0]==1 && wire[4]==5 && wire[8]==win_a && wire[12]==NWC_TEXT &&
           wire[28]==sizeof txt && !memcmp(wire+32,txt,sizeof txt) && wire[37]==0 && wire[63]==0);
    assert(nwc_receive_wire(&ipc,pid_a,16,65536,0,wire)==NWC_EMPTY && wire[4]==5);

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
    assert(nwc_publish_pixel(&ipc,pid_b,32,131072,win_a,0xabcdef)==NWC_DENIED);
    assert(nwc_publish_pixel(&ipc,pid_a,99,65536,0,0xabcdef)==NWC_DENIED);
    assert(nwc_publish_pixel(&ipc,pid_a,16,65536,UINT64_C(0x100000001),0xabcdef)==NWC_INVALID);
    assert(nwc_publish_pixel(&ipc,pid_a,16,65536,0,0x1000000)==NWC_INVALID);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK && tile.generation==2);
    assert(nwc_publish_pixel(&ipc,pid_a,16,65536,0,0x123456)==NWC_OK);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK && tile.generation==3 && tile.pixel[0]==0x123456);
    const uint32_t block[4]={0x102030,0x405060,0x708090,0xa0b0c0};
    const uint32_t bad[4]={0,0,0,0x1000000};
    assert(nwc_publish_block(&ipc,pid_a,16,65536,0,bad,2,2)==NWC_INVALID);
    assert(nwc_publish_block(&ipc,pid_a,16,65536,0,block,17,2)==NWC_INVALID);
    assert(nwc_publish_block(&ipc,pid_b,32,131072,win_a,block,2,2)==NWC_DENIED);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK && tile.generation==3);
    assert(nwc_publish_block(&ipc,pid_a,16,65536,0,block,2,2)==NWC_OK);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK && tile.generation==4 &&
           tile.width==2 && tile.height==2 && !memcmp(tile.pixel,block,sizeof block) && tile.pixel[4]==0);
    /* Wire is fully validated before publication; failures preserve
     * generation and previous frame. No pointer from the wire is used. */
    struct nwc_rect frame={0,0,500,300,0x123456};
    assert(nwc_publish_frame(&ipc,pid_a,16,65536,win_a,&frame,1)==NWC_OK);
    uint64_t gen=999;
    assert(nwc_frame_generation(&ipc,pid_b,32,131072,win_a,&gen)==NWC_DENIED&&gen==999);
    assert(nwc_frame_generation(&ipc,pid_a,16,65536,0,&gen)==NWC_OK&&gen==5);
    uint8_t patch[NWC_PATCH_BYTES]={0};
    patch[0]=1;patch[4]=5;patch[8]=16;patch[12]=16;patch[16]=16;patch[20]=16;
    patch[24]=0x33;patch[25]=0x22;patch[26]=0x11;
    gen=999;
    for(size_t n=0;n<NWC_PATCH_BYTES;++n)
        assert(nwc_patch_wire(&ipc,pid_a,16,65536,0,patch,n,&gen)==NWC_INVALID&&gen==999);
    patch[0]=2;
    assert(nwc_patch_wire(&ipc,pid_a,16,65536,0,patch,sizeof patch,&gen)==NWC_INVALID&&gen==999);
    patch[0]=1;patch[27]=1;
    assert(nwc_patch_wire(&ipc,pid_a,16,65536,0,patch,sizeof patch,&gen)==NWC_INVALID&&gen==999);
    patch[27]=0;
    assert(nwc_patch_wire(&ipc,pid_b,32,131072,win_a,patch,sizeof patch,&gen)==NWC_DENIED&&gen==999);
    assert(nwc_patch_wire(&ipc,pid_a,16,65536,0,patch,sizeof patch,&gen)==NWC_OK&&gen==6);
    assert(nwc_patch_wire(&ipc,pid_a,16,65536,0,patch,sizeof patch,&gen)==NWC_STALE&&gen==6);
    patch[4]=6;
    assert(nwc_patch_wire(&ipc,pid_a,16,65536,0,patch,sizeof patch,&gen)==NWC_OK&&gen==6);
    unsigned latest=0;
    const struct nwc_patch *view=nwc_visible_patches(&ipc,win_a,&latest);
    assert(view&&latest&&view[latest-1].pixel[0]==0x112233);
    uint8_t text[NWC_TEXT_HEADER+NWC_TEXT_RUN_BYTES]={0};
    text[0]=1;text[4]=6;text[8]=1;text[12]=4;text[16]=6;text[20]=90;
    text[24]=0xff;text[25]=0xff;text[26]=0xff;text[32]=1;
    text[36]=0x21;text[37]=4; /* Cyrillic capital Es */
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,text,sizeof text,&gen)==NWC_NOT_READY&&gen==6);
    assert(nwc_publish_frame(&ipc,pid_a,16,65536,0,&frame,1)==NWC_OK);
    text[4]=7;gen=999;
    for(size_t n=0;n<sizeof text;++n)
        assert(nwc_text_wire(&ipc,pid_a,16,65536,0,text,n,&gen)==NWC_INVALID&&gen==999);
    assert(nwc_text_wire(&ipc,pid_b,32,131072,win_a,text,sizeof text,&gen)==NWC_DENIED&&gen==999);
    text[40]=1;
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,text,sizeof text,&gen)==NWC_INVALID&&gen==999);
    text[40]=0;
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,text,sizeof text,&gen)==NWC_OK&&gen==8);
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,text,sizeof text,&gen)==NWC_STALE&&gen==8);
    text[4]=8;
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,text,sizeof text,&gen)==NWC_OK&&gen==8);
    unsigned text_count=0;
    const struct nwc_text_run *line=nwc_visible_text(&ipc,win_a,&text_count);
    assert(line&&text_count==1&&line[0].cp[0]==0x421);
    text[8]=0;
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,text,NWC_TEXT_HEADER,&gen)==NWC_OK&&gen==9);
    assert(nwc_visible_text(&ipc,win_a,&text_count)&&!text_count);
    assert(nwc_publish_block(&ipc,pid_a,16,65536,0,block,2,2)==NWC_OK);
    assert(!nwc_visible_patches(&ipc,win_a,&latest));
    assert(ws_compositor_restart(&windows)==2);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK && tile.generation==10);
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
    assert(nwc_publish_pixel(&ipc,pid_a,16,65536,win_b,0x123456)==NWC_DENIED);
    assert(nwc_publish_pixel(&ipc,pid_b,32,131072,win_b,0x654321)==NWC_OK);
    assert(nwc_visible_tile(&ipc,win_b,&tile)==NWC_OK && tile.pixel[0]==0x654321);
    assert(usr_lock(&users)==USR_OK);
    assert(nwc_send_key(&ipc,win_b,'Z',0,true)==NWC_DENIED);
    assert(nwc_visible_tile(&ipc,win_b,&tile)==NWC_DENIED);
    assert(nwc_publish_pixel(&ipc,pid_b,32,131072,0,0x010203)==NWC_DENIED);
    assert(usr_select(&users,sid_a)==USR_OK);
    assert(nwc_visible_tile(&ipc,win_a,&tile)==NWC_OK);
    assert(nwc_visible_tile(&ipc,win_b,&tile)==NWC_DENIED);
    assert(nwc_publish_tile(&ipc,pid_b,32,131072,win_b,pixels,1,1)==NWC_DENIED);
    /* Closing a window immediately revokes its IPC; port will be zeroed when
     * the trusted compositor calls reconcile. */
    assert(nwc_publish_frame(&ipc,pid_a,16,65536,0,&frame,1)==NWC_OK);
    assert(nwc_frame_generation(&ipc,pid_a,16,65536,0,&gen)==NWC_OK&&gen==11);
    static uint8_t expanded[NWC_TEXT_WIRE_MAX+NWC_TEXT_RUN_BYTES];
    expanded[0]=1;expanded[4]=11;expanded[8]=NWC_TEXT_RUNS;
    for(unsigned i=0;i<NWC_TEXT_RUNS;++i){
        uint8_t *r=expanded+NWC_TEXT_HEADER+i*NWC_TEXT_RUN_BYTES;
        unsigned x=(i%4)*100;
        r[0]=(uint8_t)x;r[1]=(uint8_t)(x>>8);r[4]=(uint8_t)((i/4)*30);r[8]=90;
        r[12]=255;r[13]=255;r[14]=255;r[20]=1;r[24]=(uint8_t)('A'+i%26);
    }
    gen=999;
    assert(nwc_text_wire(&ipc,pid_b,32,131072,win_a,expanded,NWC_TEXT_WIRE_MAX,&gen)==NWC_DENIED&&gen==999);
    expanded[NWC_TEXT_WIRE_MAX-1]=1;
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,expanded,NWC_TEXT_WIRE_MAX,&gen)==NWC_INVALID&&gen==999);
    assert(nwc_visible_text(&ipc,win_a,&text_count)&&!text_count);
    expanded[NWC_TEXT_WIRE_MAX-1]=0;
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,expanded,NWC_TEXT_WIRE_MAX,&gen)==NWC_OK&&gen==12);
    line=nwc_visible_text(&ipc,win_a,&text_count);
    assert(line&&text_count==32&&line[31].cp[0]=='F'&&line[31].x==300);
    expanded[4]=12;
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,expanded,NWC_TEXT_WIRE_MAX,&gen)==NWC_OK&&gen==12);
    expanded[8]=33;
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,expanded,sizeof expanded,&gen)==NWC_INVALID&&gen==12);
    assert(nwc_visible_text(&ipc,win_a,&text_count)&&text_count==32);
    expanded[8]=0;
    assert(nwc_text_wire(&ipc,pid_a,16,65536,0,expanded,NWC_TEXT_HEADER,&gen)==NWC_OK&&gen==13);
    assert(nwc_visible_text(&ipc,win_a,&text_count)&&!text_count);
    puts("EXPLORER124 32 text runs atomic publication and refusal PASS");
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
