#include <styleos/native_desktop_launch.h>
#include <styleos/native_desktop_bridge.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct pr_table registry;
static struct ws_service windows;
static struct dpr_service processes;
static struct usr_runtime sessions;
static struct dfo_owner frames;
static struct dvm_storage storage;
static struct nla_stage launch;
static struct nwc_service ipc;
static struct ndl_stage coordinator;
static uint32_t first_session,second_session;
static unsigned painted;
static uint32_t last_pixel;
static void paint(void *unused,int x,int y,uint32_t rgb)
{
    (void)unused;
    assert(x==20 && y==30);
    ++painted;last_pixel=rgb;
}

static void setup(void)
{
    memset(&registry,0,sizeof registry);
    memset(&windows,0,sizeof windows);
    memset(&processes,0,sizeof processes);
    memset(&sessions,0,sizeof sessions);
    memset(&frames,0,sizeof frames);
    memset(&storage,0,sizeof storage);
    memset(&launch,0,sizeof launch);
    memset(&ipc,0,sizeof ipc);
    memset(&coordinator,0,sizeof coordinator);
    pr_init(&registry);ws_init(&windows);
    assert(dpr_start(&processes,&registry,&windows,0x1000)==DPR_OK);
    assert(usr_init(&sessions,&registry,&windows)==USR_OK);
    assert(nwc_start(&ipc,&registry,&sessions,&windows)==NWC_OK);
    struct dfo_claims grants={0};
    grants.table_pa=0x1000000;
    grants.kernel_count=2;
    grants.kernel[0]=(struct dvm_region){0x200000,0x200000,2,DVM_EXEC};
    grants.kernel[1]=(struct dvm_region){0x202000,0x202000,2,DVM_WRITE};
    for(unsigned s=0;s<DVM_USER_SLOTS;++s)
        for(unsigned p=0;p<DVM_USER_PAGES;++p)
            grants.user_pa[s][p]=0x300000u+(s*3u+p)*DVM_PAGE_SIZE;
    assert(dfo_prepare(&frames,&storage,&grants)==DFO_OK);
    assert(nla_init(&launch,&processes,&frames,&sessions)==NLA_OK);
    assert(ndl_init(&coordinator,&launch,&ipc)==NDL_OK);
    assert(ndl_init(&coordinator,&launch,&ipc)==NDL_CONFLICT);
    assert(pr_snapshot(&registry,0,0,true)==0);
    assert(nwc_port_count(&ipc)==0);
    assert(usr_create(&sessions,101,&first_session)==USR_OK);
    assert(usr_create(&sessions,202,&second_session)==USR_OK);
    assert(usr_select(&sessions,first_session)==USR_OK);
}
static struct nla_request request(unsigned slot,uint32_t session,uint64_t handle)
{
    struct nla_request r={0};
    r.slot=slot;r.session_id=session;r.hardware_context=handle;
    r.label="Example.tea";r.window_title="Example";
    r.bounds=(struct ws_bounds){20,30,250,180};
    return r;
}
static void test_two_processes(void)
{
    setup();
    struct nla_request a=request(0,first_session,11),b=request(1,second_session,12);
    struct ndl_result aout={0},bout={0};
    assert(ndl_admit(&coordinator,&a,0x1000,&aout)==NDL_OK);
    assert(aout.pid!=0 && aout.window_id!=0 && aout.lease.pid==aout.pid);
    assert(nwc_port_count(&ipc)==1 && pr_find(&registry,aout.pid));
    assert(ndl_admit(&coordinator,&a,0x1000,&bout)==NDL_CONFLICT);
    assert(ndl_reap(&coordinator,0,11,0x1000)==NDL_NOT_TERMINATED);
    uint32_t pix[NWC_TILE_PIXELS]={0};pix[0]=0x00123456;
    assert(nwc_publish_tile(&ipc,aout.pid,11,frames.roots.root[1],
                            aout.window_id,pix,1,1)==NWC_OK);
    struct nwc_tile tile={0};
    assert(nwc_visible_tile(&ipc,aout.window_id,&tile)==NWC_OK);
    assert(tile.pixel[0]==0x00123456 && tile.generation==1);
    painted=0;last_pixel=0;
    assert(ndb_paint(&ipc,800,550,paint,0)==1);
    assert(painted==1 && last_pixel==0x00123456);
    assert(nwc_send_key(&ipc,aout.window_id,'Q',0,true)==NWC_OK);
    struct nwc_event ev={0};
    assert(nwc_receive(&ipc,aout.pid,11,frames.roots.root[1],
                       aout.window_id,&ev)==NWC_OK && ev.code=='Q');
    assert(ws_compositor_restart(&windows)==2);
    assert(nwc_visible_tile(&ipc,aout.window_id,&tile)==NWC_OK);
    assert(usr_select(&sessions,second_session)==USR_OK);
    assert(nwc_visible_tile(&ipc,aout.window_id,&tile)==NWC_DENIED);
    painted=0;
    assert(ndb_paint(&ipc,800,550,paint,0)==0 && painted==0);
    assert(ndl_admit(&coordinator,&b,0x1000,&bout)==NDL_OK);
    assert(bout.pid>aout.pid && bout.window_id>aout.window_id);
    assert(nwc_port_count(&ipc)==2);
    assert(nwc_publish_tile(&ipc,aout.pid,11,frames.roots.root[1],
                            aout.window_id,pix,1,1)==NWC_DENIED);
    assert(nwc_publish_tile(&ipc,bout.pid,12,frames.roots.root[2],
                            bout.window_id,pix,1,1)==NWC_OK);
    assert(ndl_reap(&coordinator,0,11,0x1000)==NDL_NOT_TERMINATED);
    unsigned selected=99;uint32_t pid=0;
    assert(dpr_dispatch(&processes,&selected,&pid)==PS_OK && pid==aout.pid);
    assert(ndl_reap(&coordinator,0,11,0x1000)==NDL_NOT_TERMINATED);
    assert(dpr_observe(&processes,0,11,frames.roots.root[1],0x1000,PS_FAULT)==PS_OK);
    assert(ndl_reap(&coordinator,0,11,0x1000)==NDL_OK);
    assert(!pr_find(&registry,aout.pid) && !ws_find(&windows,aout.window_id));
    assert(nwc_port_count(&ipc)==1 && nwc_visible_tile(&ipc,aout.window_id,&tile)==NWC_MISSING);
    assert(nwc_visible_tile(&ipc,bout.window_id,&tile)==NWC_OK);
    painted=0;
    assert(ndb_paint(&ipc,800,550,paint,0)==1 && painted==1);
    assert(ndl_reap(&coordinator,0,11,0x1000)==NDL_STALE);
    assert(dpr_dispatch(&processes,&selected,&pid)==PS_OK && pid==bout.pid);
    assert(dpr_observe(&processes,1,12,frames.roots.root[2],0x1000,PS_EXIT)==PS_OK);
    assert(ndl_reap(&coordinator,1,12,0x1000)==NDL_OK);
    assert(nwc_port_count(&ipc)==0 && pr_snapshot(&registry,0,0,true)==0);
    assert(dfo_close(&frames,0x1000)==DFO_OK);
}
static void test_port_claim_fail_closed(void)
{
    setup();
    /* Fault injection: eight occupied capability slots with stale IDs. The
     * broker must not strand a READY PID/window when claim returns FULL. */
    for(unsigned i=0;i<NWC_PORTS;++i)ipc.ports[i].window_id=1000+i;
    struct nla_request a=request(0,first_session,51);
    struct ndl_result result={0};
    assert(ndl_admit(&coordinator,&a,0x1000,&result)==NDL_PORT_DENIED);
    assert(!result.pid && !result.window_id);
    assert(pr_snapshot(&registry,0,0,true)==0 && ws_snapshot(&windows,0,0)==0);
    assert(!launch.entries[0].committed && !coordinator.records[0].active);
    assert(frames.lease[0].revoked && frames.roots.root[1]==0);
    assert(storage.table[1][0][0]==0);
    assert(nwc_port_count(&ipc)==0);
    assert(ndl_admit(&coordinator,&a,0x1000,&result)==NDL_LAUNCH_DENIED);
    assert(registry.next_pid==2); /* never reuse the failed PID */
}
static void test_not_ready_and_wrong_session(void)
{
    setup();
    struct nla_request r=request(0,second_session,25);
    struct ndl_result result={0};
    assert(ndl_admit(&coordinator,&r,0x1000,&result)==NDL_LAUNCH_DENIED);
    assert(nwc_port_count(&ipc)==0 && pr_snapshot(&registry,0,0,true)==0);
    assert(frames.lease[0].revoked);
    setup();
    r=request(0,first_session,33);
    assert(ndl_admit(&coordinator,&r,0x2000,&result)==NDL_LAUNCH_DENIED);
    assert(!frames.lease[0].revoked);
    assert(ndl_admit(&coordinator,&r,0x1000,&result)==NDL_OK);
    assert(ndl_reap(&coordinator,0,33,0x2000)==NDL_NOT_TERMINATED);
    assert(coordinator.records[0].active);
    assert(pr_transition(&registry,result.pid,33,PR_EXITED)==PR_OK);
    assert(ndl_reap(&coordinator,0,33,0x2000)==NDL_ROLLBACK_FAILED);
    assert(ndl_reap(&coordinator,0,44,0x1000)==NDL_STALE);
    assert(ndl_reap(&coordinator,0,33,0x1000)==NDL_OK);
}
int main(void)
{
    test_two_processes();
    test_port_claim_fail_closed();
    test_not_ready_and_wrong_session();
    puts("STYLEOS 775 NATIVE DESKTOP ADMISSION HOST PASS: actual PID, session, frame root, IPC port, fault cleanup and fail-closed rollback");
    return 0;
}
