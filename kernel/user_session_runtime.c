#include <styleos/user_session_runtime.h>

static struct usr_runtime global_sessions;
static void clear(void *p, size_t n)
{
    unsigned char *b = (unsigned char *)p;
    for (size_t i=0;i<n;++i) b[i]=0;
}
static bool live(const struct pr_entry *p)
{
    return p && (p->state==PR_READY || p->state==PR_RUNNING ||
                 p->state==PR_WAITING);
}
const struct usr_session *usr_find(const struct usr_runtime *s, uint32_t id)
{
    if (!s || !s->ready || !id) return 0;
    for (unsigned i=0;i<USR_MAX_SESSIONS;++i)
        if (s->sessions[i].id == id) return &s->sessions[i];
    return 0;
}
static struct usr_session *session_mut(struct usr_runtime *s,uint32_t id)
{
    return (struct usr_session *)usr_find(s,id);
}
static const struct usr_owner *owner(const struct usr_runtime *s,uint32_t pid)
{
    if(!s || !s->ready || !pid) return 0;
    for(unsigned i=0;i<PR_MAX_PROCESSES;++i)
        if(s->owners[i].pid==pid) return &s->owners[i];
    return 0;
}
enum usr_status usr_init(struct usr_runtime *s,struct pr_table *p,
                         struct ws_service *w)
{
    if (!s || !p || !w) return USR_INVALID;
    if (s->ready) return USR_BUSY;
    if (pr_snapshot(p,0,0,true) || ws_snapshot(w,0,0)) return USR_BUSY;
    clear(s,sizeof(*s));
    s->processes=p; s->windows=w; s->next_session=1; s->ready=true;
    return USR_OK;
}
enum usr_status usr_create(struct usr_runtime *s,uint64_t account_id,
                           uint32_t *session_id)
{
    if(!s || !s->ready || !session_id || !account_id) return USR_INVALID;
    if(!s->next_session || s->next_session==UINT32_MAX) return USR_EXHAUSTED;
    struct usr_session *free_slot=0;
    for(unsigned i=0;i<USR_MAX_SESSIONS;++i){
        struct usr_session *t=&s->sessions[i];
        if(t->id && t->account_id==account_id) return USR_BUSY;
        if(!t->id && !free_slot) free_slot=t;
    }
    if(!free_slot) return USR_FULL;
    free_slot->id=s->next_session++;
    free_slot->account_id=account_id;
    free_slot->state=USR_LOCKED;
    *session_id=free_slot->id;
    return USR_OK;
}
enum usr_status usr_select(struct usr_runtime *s,uint32_t id)
{
    if(!s || !s->ready || !id) return USR_INVALID;
    struct usr_session *t=session_mut(s,id);
    if(!t) return USR_MISSING;
    if(s->selected_session && s->selected_session!=id){
        struct usr_session *old=session_mut(s,s->selected_session);
        if(!old || old->state!=USR_ACTIVE) return USR_STALE;
        old->state=USR_LOCKED;
    }
    t->state=USR_ACTIVE;
    s->selected_session=id;
    return USR_OK;
}
enum usr_status usr_lock(struct usr_runtime *s)
{
    if(!s || !s->ready) return USR_INVALID;
    if(!s->selected_session) return USR_MISSING;
    struct usr_session *t=session_mut(s,s->selected_session);
    if(!t || t->state!=USR_ACTIVE) return USR_STALE;
    t->state=USR_LOCKED;
    s->selected_session=0;
    return USR_OK;
}
enum usr_status usr_close(struct usr_runtime *s,uint32_t id)
{
    if(!s || !s->ready || !id) return USR_INVALID;
    struct usr_session *t=session_mut(s,id);
    if(!t) return USR_MISSING;
    if(t->state==USR_ACTIVE || s->selected_session==id) return USR_BUSY;
    for(unsigned i=0;i<PR_MAX_PROCESSES;++i)
        if(s->owners[i].pid && s->owners[i].session_id==id)
            return USR_BUSY; /* must reconcile, never silently kill an app */
    clear(t,sizeof(*t));
    return USR_OK;
}
enum usr_status usr_bind(struct usr_runtime *s,uint32_t sid,
                         uint32_t pid,uint64_t handle)
{
    if(!s || !s->ready || !sid || !pid || !handle) return USR_INVALID;
    const struct usr_session *sess=usr_find(s,sid);
    if(!sess || sess->state!=USR_ACTIVE || s->selected_session!=sid)
        return USR_DENIED;
    const struct pr_entry *p=pr_find(s->processes,pid);
    if(!live(p) || p->type!=PR_NATIVE_USER || p->scheduler_handle!=handle)
        return USR_DENIED;
    if(owner(s,pid)) return USR_BUSY;
    if(p->parent_pid){
        const struct usr_owner *parent=owner(s,p->parent_pid);
        if(!parent || parent->session_id!=sid ||
           !pr_is_live(s->processes,parent->pid)) return USR_DENIED;
    }
    struct usr_owner *free_slot=0;
    for(unsigned i=0;i<PR_MAX_PROCESSES;++i){
        struct usr_owner *o=&s->owners[i];
        if(o->pid && o->scheduler_handle==handle) return USR_STALE;
        if(!o->pid && !free_slot) free_slot=o;
    }
    if(!free_slot) return USR_FULL;
    free_slot->pid=pid;
    free_slot->session_id=sid;
    free_slot->scheduler_handle=handle;
    return USR_OK;
}
size_t usr_reconcile(struct usr_runtime *s)
{
    if(!s || !s->ready) return 0;
    ws_reap_dead_owners(s->windows,s->processes);
    size_t removed=0;
    for(unsigned i=0;i<PR_MAX_PROCESSES;++i){
        struct usr_owner *o=&s->owners[i];
        if(!o->pid) continue;
        const struct pr_entry *p=pr_find(s->processes,o->pid);
        if(!live(p) || p->scheduler_handle!=o->scheduler_handle){
            clear(o,sizeof(*o)); ++removed;
        }
    }
    return removed;
}
size_t usr_visible_windows(const struct usr_runtime *s,
                           struct ws_window *out,size_t capacity)
{
    if(!s || !s->ready || (capacity && !out) || !s->selected_session) return 0;
    size_t n=0;
    for(unsigned i=0;i<WS_MAX_WINDOWS;++i){
        const struct ws_window *w=&s->windows->windows[i];
        if(!w->id || w->role!=WS_APP_WINDOW) continue;
        const struct usr_owner *o=owner(s,w->owner_pid);
        if(!o || o->session_id!=s->selected_session ||
           !pr_is_live(s->processes,o->pid)) continue;
        if(n<capacity) out[n]=*w;
        ++n;
    }
    return n;
}
enum usr_status usr_processes(const struct usr_runtime *s,uint32_t caller,
                              struct usr_process_row *out,size_t cap,
                              size_t *total)
{
    if(!s || !s->ready || !total || (cap && !out) || !caller) return USR_INVALID;
    const struct usr_owner *self=owner(s,caller);
    if(!self || !pr_is_live(s->processes,caller) ||
       !usr_find(s,self->session_id)) return USR_DENIED;
    size_t count=0;
    for(unsigned i=0;i<PR_MAX_PROCESSES;++i){
        const struct usr_owner *o=&s->owners[i];
        if(!o->pid || o->session_id!=self->session_id) continue;
        const struct pr_entry *p=pr_find(s->processes,o->pid);
        if(!live(p) || p->scheduler_handle!=o->scheduler_handle) continue;
        if(count<cap){
            struct usr_process_row *r=&out[count];
            clear(r,sizeof(*r));
            r->pid=p->pid; r->parent_pid=p->parent_pid; r->state=p->state;
            for(unsigned j=0;j<PR_LABEL_BYTES;++j){
                r->label[j]=p->label[j]; if(!p->label[j]) break;
            }
        }
        ++count;
    }
    *total=count;
    return USR_OK;
}
enum usr_status usr_global_start(void)
{
    return usr_init(&global_sessions,pr_global(),ws_global());
}
const struct usr_runtime *usr_global(void)
{
    return global_sessions.ready ? &global_sessions : 0;
}
