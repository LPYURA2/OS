#include <styleos/native_desktop_launch.h>

static void erase(void *dst,size_t size)
{
    unsigned char *bytes=(unsigned char *)dst;
    for(size_t i=0;i<size;++i)bytes[i]=0;
}
static bool wired(const struct ndl_stage *s)
{
    return s && s->ready && s->launch && s->launch->ready &&
           s->channel && s->channel->ready && s->launch->processes &&
           s->launch->frames && s->launch->sessions &&
           s->channel->registry==s->launch->sessions->processes &&
           s->channel->windows==s->launch->sessions->windows &&
           s->channel->sessions==s->launch->sessions;
}
enum ndl_status ndl_init(struct ndl_stage *s,struct nla_stage *launch,
                         struct nwc_service *channel)
{
    if(!s || !launch || !channel)return NDL_ARGUMENT;
    if(s->ready)return NDL_CONFLICT;
    if(!launch->ready || !channel->ready ||
       !launch->sessions || !launch->processes || !launch->frames ||
       launch->sessions->processes!=channel->registry ||
       launch->sessions->windows!=channel->windows ||
       launch->sessions!=channel->sessions ||
       launch->processes->dispatcher.current!=-1 ||
       pr_snapshot(channel->registry,0,0,true)!=0 ||
       nwc_port_count(channel)!=0)return NDL_NOT_READY;
    for(unsigned i=0;i<DVM_USER_SLOTS;++i)
        if(launch->entries[i].committed)return NDL_NOT_READY;
    erase(s,sizeof(*s));
    s->launch=launch;s->channel=channel;s->ready=true;
    return NDL_OK;
}
enum ndl_status ndl_admit(struct ndl_stage *s,const struct nla_request *req,
                          uint64_t observed_safe_root,struct ndl_result *result)
{
    if(!req || !result || req->slot>=DVM_USER_SLOTS)return NDL_ARGUMENT;
    if(!wired(s))return NDL_NOT_READY;
    if(s->records[req->slot].active)return NDL_CONFLICT;
    struct nla_result result_tmp={0};
    const enum nla_status admission=nla_commit(s->launch,req,observed_safe_root,
                                                &result_tmp);
    if(admission==NLA_ROLLBACK_FAILED)return NDL_ROLLBACK_FAILED;
    if(admission!=NLA_OK)return NDL_LAUNCH_DENIED;
    const unsigned slot=req->slot;
    const uint64_t user_root=s->launch->frames->roots.root[slot+1];
    if(nwc_claim(s->channel,result_tmp.pid,req->hardware_context,
                 user_root,result_tmp.window_id)!=NWC_OK){
        /* nla_commit admitted a READY process but nobody has dispatched it.
         * Fail closed, do NOT keep a window without its IPC capability. */
        const enum pr_status state=pr_transition(
            s->launch->sessions->processes,result_tmp.pid,
            req->hardware_context,PR_EXITED);
        if(state!=PR_OK || nla_finalize(s->launch,slot,req->hardware_context,
                                         observed_safe_root)!=NLA_OK)
            return NDL_ROLLBACK_FAILED;
        (void)nwc_reconcile(s->channel);
        return NDL_PORT_DENIED;
    }
    s->records[slot]=(struct ndl_record){result_tmp.pid,result_tmp.window_id,
                                        req->hardware_context,true};
    *result=(struct ndl_result){result_tmp.pid,result_tmp.window_id,
                                result_tmp.ticket};
    return NDL_OK;
}
enum ndl_status ndl_reap(struct ndl_stage *s,unsigned slot,
                         uint64_t hardware_context,uint64_t observed_safe_root)
{
    if(slot>=DVM_USER_SLOTS || !hardware_context)return NDL_ARGUMENT;
    if(!wired(s))return NDL_NOT_READY;
    struct ndl_record *r=&s->records[slot];
    if(!r->active || r->hardware_context!=hardware_context ||
       !s->launch->entries[slot].committed ||
       s->launch->entries[slot].pid!=r->pid ||
       s->launch->entries[slot].window_id!=r->window_id)
        return NDL_STALE;
    const struct pr_entry *p=pr_find(s->channel->registry,r->pid);
    if(!p || (p->state!=PR_EXITED && p->state!=PR_FAULTED))
        return NDL_NOT_TERMINATED;
    if(nla_finalize(s->launch,slot,hardware_context,observed_safe_root)!=NLA_OK)
        return NDL_ROLLBACK_FAILED;
    (void)nwc_reconcile(s->channel);
    erase(r,sizeof(*r));
    return NDL_OK;
}
size_t ndl_reconcile_ports(struct ndl_stage *s)
{ return wired(s)?nwc_reconcile(s->channel):0; }
