#include <styleos/user_ipc_773.h>

int ipc773_validate(const struct ipc773_request *request,uint32_t window)
{
    return request && window && request->version==IPC773_VERSION &&
        request->operation==IPC773_GET_KEY && request->window_id==window &&
        request->reply_size==sizeof(struct ipc773_reply);
}
int ipc773_start(struct ipc773_broker *b,struct nsh_stage *s)
{
    if(!b || !s || !s->ready)return 0;
    *b=(struct ipc773_broker){0};
    if(nwc_start(&b->channel,s->users.processes,&s->users,&s->windows)!=NWC_OK)
        return 0;
    b->stage=s;
    return 1;
}
int ipc773_arm(struct ipc773_broker *b)
{
    if(!b || !b->stage || !b->stage->ready || b->returns || b->denied)return 0;
    struct nsh_stage *s=b->stage;
    if(!s->pid[0] || !s->pid[1] || s->user_root[0]==s->user_root[1] ||
       s->window[0]!=1 || usr_select(&s->users,s->sid[0])!=USR_OK ||
       nwc_claim(&b->channel,s->pid[0],s->handle[0],s->user_root[0],s->window[0])!=NWC_OK ||
       nwc_send_key(&b->channel,s->window[0],IPC773_KEY_CODE,0,true)!=NWC_OK)
        return 0;
    return nwc_port_count(&b->channel)==1;
}
int ipc773_fetch(struct ipc773_broker *b,uint64_t root,
                 const struct ipc773_request *request,struct ipc773_reply *out)
{
    if(!b || !b->stage || !b->stage->ready || !request || !out || !root ||
       b->returns || !ipc773_validate(request,b->stage->window[0]) ||
       root!=b->stage->user_root[0])return 0;
    const struct nsh_stage *s=b->stage;
    struct nwc_event event={0};
    if(nwc_receive(&b->channel,s->pid[0],s->handle[0],root,s->window[0],&event)!=NWC_OK ||
       event.kind!=NWC_KEY || event.code!=IPC773_KEY_CODE || !event.down ||
       event.sequence!=1 || event.window_id!=s->window[0])return 0;
    *out=(struct ipc773_reply){0,event.sequence,event.code,(uint32_t)event.kind};
    ++b->returns;
    return 1;
}
