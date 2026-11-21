#include <styleos/user_ipc_773.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct pr_table procs;
static struct nsh_stage session;
static struct ipc773_broker ipc;
static uint32_t admit(uint64_t handle,uint64_t root)
{
    struct pr_admission a={PR_NATIVE_USER,0,handle,root,0,"CPL3 test"};
    uint32_t pid=0;
    assert(pr_admit(&procs,&a,&pid)==PR_OK);
    return pid;
}
int main(void)
{
    pr_init(&procs);
    assert(nsh_init(&session,&procs));
    assert(ipc773_start(&ipc,&session));
    const uint32_t a=admit(16,0x10000),b=admit(32,0x20000);
    assert(nsh_attach(&session,0,a,16,0x10000));
    assert(nsh_attach(&session,1,b,32,0x20000));
    assert(nsh_review(&session));
    assert(ipc773_arm(&ipc));
    assert(nwc_port_count(&ipc.channel)==1);
    struct ipc773_request req={1,1,session.window[0],16};
    assert(ipc773_validate(&req,session.window[0]));
    struct ipc773_reply out={0xaaaabbbbu,0xccccddddu,0,0};
    assert(!ipc773_fetch(&ipc,0x20000,&req,&out));
    assert(out.status==0xaaaabbbbu && out.sequence==0xccccddddu);
    assert(ipc.channel.ports[0].length==1);
    req.version=2;
    assert(!ipc773_validate(&req,session.window[0]));
    assert(!ipc773_fetch(&ipc,0x10000,&req,&out));
    assert(ipc.channel.ports[0].length==1);
    req.version=1;req.window_id=session.window[1];
    assert(!ipc773_validate(&req,session.window[0]));
    req.window_id=session.window[0];
    assert(ipc773_fetch(&ipc,0x10000,&req,&out));
    assert(out.status==0 && out.sequence==1 && out.keycode==IPC773_KEY_CODE &&
           out.kind==NWC_KEY && ipc.returns==1);
    assert(ipc.channel.ports[0].length==0);
    struct ipc773_reply unchanged=out;
    assert(!ipc773_fetch(&ipc,0x10000,&req,&out));
    assert(!memcmp(&unchanged,&out,sizeof(out)));
    assert(IPC773_USER_DENIED==2 && sizeof(req)==16 && sizeof(out)==16);
    puts("IPC 773 HOST BROKER OWNERSHIP, REFUSAL AND SINGLE DELIVERY PASS");
    return 0;
}
