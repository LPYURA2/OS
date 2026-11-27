#include <styleos/deep_drop_ipc.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct pr_table processes;
static struct ws_service windows;
static struct usr_runtime users;
static struct nwc_service channel;
static struct dd_ipc ipc;
static unsigned checks;
#define CHECK(test) do { assert(test); ++checks; } while (0)
static struct dd_endpoint add_app(uint32_t session,uint64_t generation)
{
    struct dd_endpoint e={.generation=generation,.root=generation*4096};
    struct pr_admission a={PR_NATIVE_USER,0,generation,e.root,0,"Drop client"};
    CHECK(pr_admit(&processes,&a,&e.pid)==PR_OK);
    CHECK(usr_bind(&users,session,e.pid,generation)==USR_OK);
    CHECK(ws_open_app(&windows,&processes,e.pid,"Drop",(struct ws_bounds){0,0,80,60},&e.window)==WS_OK);
    CHECK(nwc_claim(&channel,e.pid,generation,e.root,e.window)==NWC_OK);
    return e;
}
static uint64_t event(struct dd_endpoint e,unsigned code)
{
    uint8_t wire[NWC_INPUT_BYTES];
    CHECK(nwc_receive_wire(&channel,e.pid,e.generation,e.root,e.window,wire)==NWC_OK);
    CHECK(wire[12]==DD_EVENT_KIND && wire[16]==code && wire[28]==8);
    for(size_t i=40;i<sizeof wire;i++)assert(!wire[i]);
    uint64_t id=0;
    for(unsigned i=0;i<8;i++)id|=(uint64_t)wire[32+i]<<(8*i);
    CHECK(id!=0);
    return id;
}
static void idle(void)
{
    struct dd_gesture none={0};
    struct dd_transfer empty={0};
    CHECK(!memcmp(&ipc.gesture,&none,sizeof none));
    CHECK(!memcmp(&ipc.broker.active,&empty,sizeof empty));
}
int main(void)
{
    pr_init(&processes);ws_init(&windows);
    CHECK(usr_init(&users,&processes,&windows)==USR_OK);
    CHECK(nwc_start(&channel,&processes,&users,&windows)==NWC_OK);
    CHECK(dd_init(&ipc.broker,&channel)==DD_OK);
    uint32_t sid,other;
    CHECK(usr_create(&users,1,&sid)==USR_OK && usr_create(&users,2,&other)==USR_OK);
    CHECK(usr_select(&users,sid)==USR_OK);
    struct dd_endpoint a=add_app(sid,7), b=add_app(sid,8), wrong=add_app(sid,9);
    uint8_t input[DD_TEXT_MAX], output[DD_TEXT_MAX+1];
    memset(input,'A',sizeof input);memset(output,0xab,sizeof output);
    uint64_t n=99;
    CHECK(dd_ipc_call(&ipc,a,DD_CALL_OFFER,1,input,8,1,&n)==DD_STALE && !n);
    CHECK(dd_ipc_gesture(&ipc,a.window,a.window,1,30)==DD_DENIED);
    CHECK(dd_ipc_gesture(&ipc,a.window,b.window,UINT64_MAX-1,3)==DD_INVALID);
    /* Queue pressure at the source neither publishes an id nor stages bytes. */
    for(unsigned i=0;i<NWC_EVENTS;i++)CHECK(nwc_send_key(&channel,a.window,1,0,true)==NWC_OK);
    CHECK(dd_ipc_gesture(&ipc,a.window,b.window,2,20)==DD_BUSY);idle();
    struct nwc_event ev;
    for(unsigned i=0;i<NWC_EVENTS;i++)CHECK(nwc_receive(&channel,a.pid,a.generation,a.root,a.window,&ev)==NWC_OK);
    /* Queue pressure after offer rolls back, scrubs bytes and burns the id. */
    CHECK(dd_ipc_gesture(&ipc,a.window,b.window,3,20)==DD_OK);
    uint64_t g=event(a,DD_REQUEST_TEXT);
    for(unsigned i=0;i<NWC_EVENTS;i++)CHECK(nwc_send_key(&channel,b.window,1,0,true)==NWC_OK);
    CHECK(dd_ipc_call(&ipc,a,DD_CALL_OFFER,g,input,sizeof input,3,&n)==DD_BUSY && !n);
    CHECK(ipc.broker.last_id==1);idle();
    for(unsigned i=0;i<NWC_EVENTS;i++)CHECK(nwc_receive(&channel,b.pid,b.generation,b.root,b.window,&ev)==NWC_OK);
    uint64_t last=1;
    for(unsigned i=0;i<100;i++){
        uint64_t now=4+i;
        CHECK(dd_ipc_gesture(&ipc,a.window,b.window,now,20)==DD_OK);
        g=event(a,DD_REQUEST_TEXT);
        CHECK(dd_ipc_call(&ipc,wrong,DD_CALL_OFFER,g,input,sizeof input,now,&n)==DD_DENIED && !n);
        struct dd_endpoint stale=a;stale.generation++;
        CHECK(dd_ipc_call(&ipc,stale,DD_CALL_OFFER,g,input,sizeof input,now,&n)==DD_DENIED);
        CHECK(dd_ipc_call(&ipc,a,DD_CALL_OFFER,g,input,sizeof input,now,&n)==DD_OK && n==last+1);
        last=n;
        CHECK(dd_ipc_call(&ipc,a,DD_CALL_OFFER,g,input,sizeof input,now,&n)==DD_STALE);
        CHECK(event(b,DD_TEXT_OFFER)==last);
        CHECK(dd_ipc_call(&ipc,wrong,DD_CALL_ACCEPT,last,0,0,now,&n)==DD_DENIED);
        CHECK(dd_ipc_call(&ipc,b,DD_CALL_ACCEPT,last,0,0,now,&n)==DD_OK);
        CHECK(dd_ipc_call(&ipc,b,DD_CALL_READ,last,output,1,now,&n)==DD_CAPACITY && !n);
        CHECK(dd_ipc_call(&ipc,b,DD_CALL_READ,last,output,DD_TEXT_MAX,now,&n)==DD_OK && n==sizeof input);
        CHECK(!memcmp(input,output,sizeof input) && output[DD_TEXT_MAX]==0xab);
        CHECK(dd_ipc_call(&ipc,b,DD_CALL_READ,last,output,DD_TEXT_MAX,now,&n)==DD_STALE && !n);
        CHECK(dd_ipc_call(&ipc,b,DD_CALL_FINISH,last,0,0,now,&n)==DD_OK);idle();
    }
    CHECK(dd_ipc_gesture(&ipc,a.window,b.window,104,3)==DD_OK);
    CHECK(dd_ipc_reconcile(&ipc,107));idle();
    g=event(a,DD_REQUEST_TEXT);
    CHECK(dd_ipc_call(&ipc,a,DD_CALL_OFFER,g,input,5,108,&n)==DD_STALE);
    CHECK(dd_ipc_gesture(&ipc,a.window,b.window,109,10)==DD_OK);
    g=event(a,DD_REQUEST_TEXT);
    CHECK(dd_ipc_call(&ipc,a,DD_CALL_OFFER,g,input,5,110,&n)==DD_OK);
    CHECK(dd_ipc_reconcile(&ipc,100));idle();
    CHECK(event(b,DD_TEXT_OFFER)==last+1);
    CHECK(dd_ipc_call(&ipc,b,DD_CALL_ACCEPT,last+1,0,0,101,&n)==DD_STALE);
    CHECK(dd_ipc_gesture(&ipc,a.window,b.window,120,8)==DD_OK);
    g=event(a,DD_REQUEST_TEXT);
    CHECK(usr_select(&users,other)==USR_OK && dd_ipc_reconcile(&ipc,121));idle();
    CHECK(usr_select(&users,sid)==USR_OK);
    CHECK(dd_ipc_call(&ipc,a,DD_CALL_OFFER,g,input,5,122,&n)==DD_STALE);
    /* Malformed notification records must also fail the runtime audit. */
    ev=(struct nwc_event){.kind=NWC_DEEP_DROP,.code=DD_TEXT_OFFER,.size=8,.bytes={1}};
    CHECK(nwc_drop_event_valid(&ev));
    ev.bytes[31]=1;CHECK(!nwc_drop_event_valid(&ev));ev.bytes[31]=0;
    ev.down=true;CHECK(!nwc_drop_event_valid(&ev));ev.down=false;
    ev.modifiers=1;CHECK(!nwc_drop_event_valid(&ev));ev.modifiers=0;
    ev.code=3;CHECK(!nwc_drop_event_valid(&ev));ev.code=DD_REQUEST_TEXT;
    ev.size=7;CHECK(!nwc_drop_event_valid(&ev));ev.size=8;
    ev.bytes[0]=0;CHECK(!nwc_drop_event_valid(&ev));
    /* 73: taskbar identity remains valid for minimized windows, but their
     * normal input/data permissions remain denied until trusted restoration. */
    struct nwc_task tasks[NWC_PORTS], tok={0};
    CHECK(nwc_task_snapshot(&channel,tasks,NWC_PORTS)==3);
    CHECK(nwc_task_snapshot(&channel,0,0)==3);
    CHECK(nwc_task_snapshot(&channel,0,1)==0);
    for(size_t i=0;i<3;++i)if(tasks[i].window_id==b.window)tok=tasks[i];
    CHECK(tok.pid==b.pid&&tok.generation==b.generation&&tok.root==b.root&&tok.session_id==sid&&!tok.minimized);
    CHECK(nwc_task_set_minimized(&channel,&tok,true)==NWC_OK);
    CHECK(nwc_task_set_minimized(&channel,&tok,false)==NWC_STALE);
    CHECK(nwc_input_ready(&channel,b.pid,b.generation,b.root,b.window)==NWC_DENIED);
    CHECK(nwc_task_snapshot(&channel,tasks,NWC_PORTS)==3);
    for(size_t i=0;i<3;++i)if(tasks[i].window_id==b.window)tok=tasks[i];
    CHECK(tok.minimized);
    struct nwc_task forged=tok;forged.root+=4096;
    CHECK(dd_ipc_task_gesture(&ipc,a.window,&forged,130,20)==DD_STALE);
    CHECK(ws_find(&windows,b.window)->minimized);
    CHECK(dd_ipc_gesture(&ipc,a.window,b.window,131,20)==DD_DENIED);
    /* Source queue full: restoration must roll back, no request published. */
    for(unsigned i=0;i<NWC_EVENTS;++i)CHECK(nwc_send_key(&channel,a.window,1,0,true)==NWC_OK);
    CHECK(dd_ipc_task_gesture(&ipc,a.window,&tok,132,20)==DD_BUSY);
    CHECK(ws_find(&windows,b.window)->minimized);idle();
    for(unsigned i=0;i<NWC_EVENTS;++i)CHECK(nwc_receive(&channel,a.pid,a.generation,a.root,a.window,&ev)==NWC_OK);
    CHECK(dd_ipc_task_gesture(&ipc,a.window,&tok,133,20)==DD_OK);
    CHECK(!ws_find(&windows,b.window)->minimized);
    CHECK(dd_ipc_task_gesture(&ipc,a.window,&tok,133,20)==DD_BUSY);
    g=event(a,DD_REQUEST_TEXT);
    CHECK(dd_ipc_call(&ipc,a,DD_CALL_OFFER,g,input,sizeof input,134,&n)==DD_OK);
    last=n;CHECK(event(b,DD_TEXT_OFFER)==last);
    CHECK(dd_ipc_call(&ipc,b,DD_CALL_ACCEPT,last,0,0,135,&n)==DD_OK);
    CHECK(dd_ipc_call(&ipc,b,DD_CALL_READ,last,output,DD_TEXT_MAX,135,&n)==DD_OK&&n==sizeof input);
    CHECK(!memcmp(input,output,sizeof input));
    CHECK(dd_ipc_call(&ipc,b,DD_CALL_FINISH,last,0,0,135,&n)==DD_OK);idle();
    CHECK(nwc_task_set_minimized(&channel,&tok,true)==NWC_STALE);
    tok.minimized=false;CHECK(nwc_task_set_minimized(&channel,&tok,true)==NWC_OK);tok.minimized=true;
    CHECK(usr_lock(&users)==USR_OK);CHECK(nwc_task_snapshot(&channel,tasks,NWC_PORTS)==0);
    CHECK(nwc_task_set_minimized(&channel,&tok,false)==NWC_DENIED);
    CHECK(usr_select(&users,other)==USR_OK);CHECK(nwc_task_snapshot(&channel,tasks,NWC_PORTS)==0);
    CHECK(nwc_task_set_minimized(&channel,&tok,false)==NWC_DENIED);
    CHECK(usr_select(&users,sid)==USR_OK);
    CHECK(ws_compositor_restart(&windows)==2);
    CHECK(nwc_task_set_minimized(&channel,&tok,false)==NWC_OK);
    tok.minimized=false;
    CHECK(ws_close_app(&windows,b.pid,b.window)==WS_OK);
    CHECK(dd_ipc_task_gesture(&ipc,a.window,&tok,136,20)==DD_DENIED);
    CHECK(nwc_task_snapshot(&channel,tasks,NWC_PORTS)==2);
    idle();
    printf("Deep Drop IPC: %u checks PASS; %zu bytes persistent incl. %u payload bytes\n",
           checks,sizeof ipc,DD_TEXT_MAX);
}
