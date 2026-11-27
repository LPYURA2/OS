#include <styleos/deep_drop_ipc.h>

static struct dd_endpoint endpoint(const struct nwc_service *channel,uint32_t window)
{
    if (channel && window) {
        for (unsigned i=0; i<NWC_PORTS; ++i) {
            const struct nwc_port *p=&channel->ports[i];
            if (p->window_id==window)
                return (struct dd_endpoint){p->owner_pid,window,p->scheduler_handle,p->user_root};
        }
    }
    return (struct dd_endpoint){0};
}
static bool owner(struct dd_endpoint a,struct dd_endpoint b)
{
    return a.pid && a.pid==b.pid && a.generation==b.generation && a.root==b.root;
}
bool dd_ipc_reconcile(struct dd_ipc *s,uint64_t now)
{
    if (!s) return false;
    /* A counter discontinuity invalidates both outstanding capabilities. */
    uint64_t check=now>=s->last_time && now ? now : UINT64_MAX;
    s->last_time=now;
    bool cleared=dd_reconcile(&s->broker,check);
    struct dd_gesture *g=&s->gesture;
    if (g->id && (check>=g->deadline || !dd_endpoint_valid(&s->broker,g->source) ||
                  !dd_endpoint_valid(&s->broker,g->target))) {
        *g=(struct dd_gesture){0};
        cleared=true;
    }
    return cleared;
}
enum dd_status dd_ipc_gesture(struct dd_ipc *s,uint32_t from,uint32_t to,
                              uint64_t now,uint64_t lifetime)
{
    if (!s || !s->broker.channel || !now || !lifetime || lifetime>UINT64_MAX-now)
        return DD_INVALID;
    dd_ipc_reconcile(s,now);
    if (s->gesture.id || s->broker.active.phase!=DD_IDLE) return DD_BUSY;
    if (s->last_gesture==UINT64_MAX) return DD_EXHAUSTED;
    struct dd_endpoint source=endpoint(s->broker.channel,from);
    struct dd_endpoint target=endpoint(s->broker.channel,to);
    if (source.pid==target.pid || !dd_endpoint_valid(&s->broker,source) ||
        !dd_endpoint_valid(&s->broker,target)) return DD_DENIED;
    uint64_t id=s->last_gesture+1;
    if (nwc_send_drop(s->broker.channel,from,DD_REQUEST_TEXT,id)!=NWC_OK)
        return DD_BUSY;
    s->gesture=(struct dd_gesture){source,target,id,now+lifetime};
    s->last_gesture=id;
    return DD_OK;
}
enum dd_status dd_ipc_task_gesture(struct dd_ipc *s,uint32_t from,const struct nwc_task *t,
                                   uint64_t now,uint64_t lifetime)
{
    if(!s||!s->broker.channel||!t||!now||!lifetime||lifetime>UINT64_MAX-now)return DD_INVALID;
    struct nwc_task observed=*t;
    struct dd_endpoint a=endpoint(s->broker.channel,from);
    if(!dd_endpoint_valid(&s->broker,a)||a.pid==observed.pid)return DD_DENIED;
    /* Reconcile before touching visibility, so a busy live transfer cannot
     * activate unrelated windows. Existing visible-only IPC checks stay intact. */
    dd_ipc_reconcile(s,now);
    if(s->gesture.id||s->broker.active.phase!=DD_IDLE)return DD_BUSY;
    if(s->last_gesture==UINT64_MAX)return DD_EXHAUSTED;
    enum nwc_status v=nwc_task_set_minimized(s->broker.channel,&observed,false);
    if(v!=NWC_OK)return v==NWC_STALE?DD_STALE:DD_DENIED;
    enum dd_status status=dd_ipc_gesture(s,from,observed.window_id,now,lifetime);
    if(status!=DD_OK&&observed.minimized){
        observed.minimized=false;
        if(nwc_task_set_minimized(s->broker.channel,&observed,true)!=NWC_OK)return DD_STALE;
    }
    return status;
}
enum dd_status dd_ipc_call(struct dd_ipc *s,struct dd_endpoint caller,unsigned op,
                           uint64_t id,uint8_t *span,size_t size,uint64_t now,uint64_t *result)
{
    if (!s || !result) return DD_INVALID;
    *result=0;
    dd_ipc_reconcile(s,now);
    if (!id || !now) return DD_INVALID;
    struct dd_service *b=&s->broker;
    if (op==DD_CALL_OFFER) {
        struct dd_gesture g=s->gesture;
        if (id!=g.id) return DD_STALE;
        if (!owner(caller,g.source)) return DD_DENIED;
        uint64_t transfer=0;
        enum dd_status status=dd_offer(b,g.source,g.target,span,size,now,g.deadline,true,&transfer);
        if (status!=DD_OK) return status;
        s->gesture=(struct dd_gesture){0};
        if (nwc_send_drop(b->channel,g.target.window,DD_TEXT_OFFER,transfer)!=NWC_OK) {
            /* Nothing was published to the target. Scrub and burn this id. */
            dd_cancel(b,g.source,transfer);
            return DD_BUSY;
        }
        *result=transfer;
        return DD_OK;
    }
    if (op!=DD_CALL_READ && (span || size)) return DD_INVALID;
    struct dd_transfer *t=&b->active;
    if (t->phase==DD_IDLE || id!=t->id) return DD_STALE;
    if (owner(caller,t->target)) caller.window=t->target.window;
    else if (owner(caller,t->source)) caller.window=t->source.window;
    else return DD_DENIED;
    switch (op) {
    case DD_CALL_ACCEPT: return dd_accept(b,caller,id,now);
    case DD_CALL_FINISH: return dd_finish(b,caller,id,now);
    case DD_CALL_CANCEL: return dd_cancel(b,caller,id);
    case DD_CALL_READ: {
        if (!span || !size || size>DD_TEXT_MAX) return DD_INVALID;
        size_t written=0;
        enum dd_status status=dd_deliver(b,caller,id,now,span,size,&written);
        if (status==DD_OK) *result=written;
        return status;
    }
    default: return DD_INVALID;
    }
}
