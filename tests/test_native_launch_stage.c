#include <styleos/native_launch_stage.h>
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
static struct dfo_claims grants;
static uint32_t alice, bob;

static void setup(void)
{
    memset(&registry,0,sizeof(registry));
    memset(&windows,0,sizeof(windows));
    memset(&processes,0,sizeof(processes));
    memset(&sessions,0,sizeof(sessions));
    memset(&frames,0,sizeof(frames));
    memset(&storage,0,sizeof(storage));
    memset(&launch,0,sizeof(launch));
    memset(&grants,0,sizeof(grants));
    pr_init(&registry); ws_init(&windows);
    assert(dpr_start(&processes,&registry,&windows,0x1000) == DPR_OK);
    assert(usr_init(&sessions,&registry,&windows) == USR_OK);
    grants.table_pa=0x1000000;
    grants.kernel_count=2;
    grants.kernel[0]=(struct dvm_region){0x200000,0x200000,2,DVM_EXEC};
    grants.kernel[1]=(struct dvm_region){0x202000,0x202000,2,DVM_WRITE};
    for(unsigned s=0;s<DVM_USER_SLOTS;++s)
        for(unsigned p=0;p<DVM_USER_PAGES;++p)
            grants.user_pa[s][p] = 0x300000u+(s*3u+p)*DVM_PAGE_SIZE;
    assert(dfo_prepare(&frames,&storage,&grants)==DFO_OK);
    assert(nla_init(&launch,&processes,&frames,&sessions)==NLA_OK);
    assert(nla_init(&launch,&processes,&frames,&sessions)==NLA_CONFLICT);
    assert(usr_create(&sessions,101,&alice)==USR_OK);
    assert(usr_create(&sessions,202,&bob)==USR_OK);
    assert(usr_select(&sessions,alice)==USR_OK);
}
static struct nla_request request(unsigned slot,uint32_t session,uint64_t handle)
{
    struct nla_request r;
    memset(&r,0,sizeof(r));
    r.slot=slot; r.session_id=session; r.hardware_context=handle;
    r.label="Editor.tea"; r.window_title="Editor";
    r.bounds=(struct ws_bounds){12,18,450,300};
    return r;
}
static void assert_clean(void)
{
    assert(pr_snapshot(&registry,0,0,true)==0);
    assert(ws_snapshot(&windows,0,0)==0);
    for(unsigned i=0;i<PR_MAX_PROCESSES;++i)
        assert(sessions.owners[i].pid==0);
    for(unsigned s=0;s<DVM_USER_SLOTS;++s)
        assert(!launch.entries[s].committed);
}
static void success_two_sessions(void)
{
    setup();
    struct nla_request a=request(0,alice,17),b=request(1,bob,18);
    struct nla_result outa={0},outb={0};
    assert(nla_commit(&launch,&a,0x1000,&outa)==NLA_OK);
    assert(outa.pid!=0 && outa.window_id!=0 && outa.ticket.pid==outa.pid);
    assert(launch.entries[0].committed);
    assert(pr_find(&registry,outa.pid)->state==PR_READY);
    assert(pr_find(&registry,outa.pid)->type==PR_NATIVE_USER);
    assert(pr_find(&registry,outa.pid)->cpu_ticks==0);
    assert(pr_find(&registry,outa.pid)->resident_bytes==0);
    assert(nla_commit(&launch,&a,0x1000,&outb)==NLA_CONFLICT);
    assert(usr_visible_windows(&sessions,0,0)==1);
    assert(ws_compositor_restart(&windows)==2);
    assert(usr_visible_windows(&sessions,0,0)==1);
    assert(usr_select(&sessions,bob)==USR_OK);
    assert(usr_visible_windows(&sessions,0,0)==0);
    assert(nla_commit(&launch,&b,0x1000,&outb)==NLA_OK);
    assert(outb.pid>outa.pid && outb.window_id>outa.window_id);
    assert(usr_visible_windows(&sessions,0,0)==1);
    assert(nla_finalize(&launch,0,17,0x1000)==NLA_NOT_TERMINATED);
    assert(nla_finalize(&launch,0,99,0x1000)==NLA_STALE);
    assert(nla_finalize(&launch,0,17,frames.roots.root[1])==NLA_CONTEXT);
    unsigned slot=9;uint32_t pid=0;
    assert(dpr_dispatch(&processes,&slot,&pid)==PS_OK && slot==0 && pid==outa.pid);
    assert(nla_finalize(&launch,0,17,0x1000)==NLA_CONTEXT);
    assert(dpr_observe(&processes,0,17,frames.roots.root[1],0x1000,PS_FAULT)==PS_OK);
    assert(nla_finalize(&launch,0,17,0x1000)==NLA_OK);
    assert(!pr_find(&registry,outa.pid));
    assert(!ws_find(&windows,outa.window_id));
    assert(!launch.entries[0].committed);
    assert(!frames.roots.root[1] && frames.lease[0].revoked);
    assert(dpr_dispatch(&processes,&slot,&pid)==PS_OK && slot==1 && pid==outb.pid);
    assert(dpr_observe(&processes,1,18,frames.roots.root[2],0x1000,PS_EXIT)==PS_OK);
    assert(nla_finalize(&launch,1,18,0x1000)==NLA_OK);
    assert(!ws_find(&windows,outb.window_id));
    assert(pr_snapshot(&registry,0,0,true)==0);
    assert(dfo_close(&frames,0x1000)==DFO_OK);
    assert(!storage.table[0][0][0]);
    assert(usr_reconcile(&sessions)==0);
    assert_clean();
}
static void rollback_session(void)
{
    setup();
    struct nla_request r=request(0,bob,31);
    struct nla_result out;memset(&out,0x5a,sizeof(out));
    assert(nla_commit(&launch,&r,0x1000,&out)==NLA_SESSION);
    assert(out.pid==UINT32_C(0x5a5a5a5a));
    assert_clean();
    assert(frames.lease[0].revoked && !frames.roots.root[1]);
    assert(!storage.table[1][0][0]);
    assert(nla_commit(&launch,&r,0x1000,&out)==NLA_CONFLICT);
    assert(registry.next_pid==2); /* real PID consumed, never reused */
}
static void rollback_window(void)
{
    setup();
    uint32_t id=0;
    for(unsigned i=0;i<WS_MAX_WINDOWS;++i)
        assert(ws_open_package_dialog(&windows,"Reviewed",WS_INSTALL_REVIEWED,
                 (struct ws_bounds){0,0,200,100},&id)==WS_OK);
    const uint32_t generation=windows.next_id;
    struct nla_request r=request(0,alice,32);
    struct nla_result out={0};
    assert(nla_commit(&launch,&r,0x1000,&out)==NLA_WINDOW);
    assert(pr_snapshot(&registry,0,0,true)==0);
    assert(ws_snapshot(&windows,0,0)==WS_MAX_WINDOWS);
    assert(windows.next_id==generation);
    assert(frames.lease[0].revoked && !frames.roots.root[1]);
    for(unsigned i=0;i<PR_MAX_PROCESSES;++i)assert(!sessions.owners[i].pid);
}
static void rollback_frames(void)
{
    setup();
    frames.next_generation=UINT32_MAX;
    struct nla_request r=request(0,alice,33);
    struct nla_result out={0};
    assert(nla_commit(&launch,&r,0x1000,&out)==NLA_FRAMES);
    assert_clean();
    assert(!frames.lease[0].bound && !frames.lease[0].revoked);
    assert(frames.roots.root[1]);
}
static void reject_untrusted_evidence(void)
{
    setup();
    struct nla_request r=request(0,alice,34);
    struct nla_result out={0};
    assert(nla_commit(&launch,&r,0x2000,&out)==NLA_CONTEXT);
    assert(nla_commit(&launch,&r,frames.roots.root[1],&out)==NLA_CONTEXT);
    assert(nla_commit(&launch,&r,0x1001,&out)==NLA_CONTEXT);
    r.hardware_context=0;
    assert(nla_commit(&launch,&r,0x1000,&out)==NLA_ARGUMENT);
    r.hardware_context=34;
    r.session_id=999;
    assert(nla_commit(&launch,&r,0x1000,&out)==NLA_SESSION);
    assert_clean();
}
static void deny_parent_from_other_session(void)
{
    setup();
    struct nla_request first=request(0,alice,41);
    struct nla_result out={0};
    assert(nla_commit(&launch,&first,0x1000,&out)==NLA_OK);
    assert(usr_select(&sessions,bob)==USR_OK);
    struct nla_request child=request(1,bob,42);
    child.parent_pid=out.pid;
    struct nla_result result={0};
    assert(nla_commit(&launch,&child,0x1000,&result)==NLA_SESSION);
    assert(frames.lease[1].revoked && !frames.roots.root[2]);
    assert(!pr_find(&registry,out.pid+1));
    assert(ws_snapshot(&windows,0,0)==1);
    assert(usr_select(&sessions,alice)==USR_OK);
    assert(usr_visible_windows(&sessions,0,0)==1);
    assert(pr_transition(&registry,out.pid,41,PR_EXITED)==PR_OK);
    assert(nla_finalize(&launch,0,41,0x1000)==NLA_OK);
    assert_clean();
}
int main(void)
{
    success_two_sessions();
    rollback_session();
    rollback_window();
    rollback_frames();
    reject_untrusted_evidence();
    deny_parent_from_other_session();
    puts("NATIVE LAUNCH STAGE HOST PASS: real PIDs, leases, sessions, window, rollback, shell restart");
    return 0;
}
