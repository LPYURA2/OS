#include <styleos/native_session_hw_770.h>

static int live_pid(const struct nsh_stage *s, unsigned i)
{
    const struct pr_entry *p=pr_find(s->users.processes,s->pid[i]);
    return p && pr_is_live(s->users.processes,s->pid[i]) &&
           p->type==PR_NATIVE_USER && p->scheduler_handle==s->handle[i] &&
           p->page_root==s->user_root[i];
}
static int visible(const struct nsh_stage *s, unsigned slot)
{
    struct ws_window windows[2]={0};
    size_t n=usr_visible_windows(&s->users,windows,2);
    return n==1 && windows[0].owner_pid==s->pid[slot] &&
           windows[0].id==s->window[slot] &&
           windows[0].role==WS_APP_WINDOW;
}
int nsh_init(struct nsh_stage *s, struct pr_table *registry)
{
    if(!s || s->ready || !registry || pr_snapshot(registry,0,0,true))return 0;
    *s=(struct nsh_stage){0};ws_init(&s->windows);
    if(usr_init(&s->users,registry,&s->windows)!=USR_OK ||
       usr_create(&s->users,UINT64_C(0x770a),&s->sid[0])!=USR_OK ||
       usr_create(&s->users,UINT64_C(0x770b),&s->sid[1])!=USR_OK ||
       !s->sid[0] || s->sid[1]==s->sid[0])return 0;
    s->ready=true;return 1;
}
int nsh_attach(struct nsh_stage *s,unsigned slot,uint32_t pid,
               uint64_t handle,uint64_t root)
{
    if(!s||!s->ready||slot>=2||!pid||!handle||!root||s->pid[slot]||
       (slot==1 && (!s->pid[0]||root==s->user_root[0])))return 0;
    const struct pr_entry *p=pr_find(s->users.processes,pid);
    if(!p||p->type!=PR_NATIVE_USER||p->state!=PR_READY||
       p->scheduler_handle!=handle||p->page_root!=root)return 0;
    if(usr_select(&s->users,s->sid[slot])!=USR_OK ||
       usr_bind(&s->users,s->sid[slot],pid,handle)!=USR_OK)return 0;
    uint32_t win=0;
    if(ws_open_app(&s->windows,s->users.processes,pid,
                   slot?"Minimal.tea":"Hardware fault probe",
                   (struct ws_bounds){10+(int)slot*30,20,640,480},&win)!=WS_OK)
        return 0;
    s->pid[slot]=pid;s->handle[slot]=handle;
    s->user_root[slot]=root;s->window[slot]=win;
    return win!=0;
}
int nsh_review(struct nsh_stage *s)
{
    if(!s || !s->ready || !live_pid(s,0) || !live_pid(s,1) ||
       s->users.selected_session!=s->sid[1] ||
       s->window[0]==s->window[1] ||
       ws_snapshot(&s->windows,0,0)!=2 || !visible(s,1) ||
       ws_close_app(&s->windows,s->pid[0],s->window[1])!=WS_DENIED ||
       ws_compositor_restart(&s->windows)!=2 ||
       !ws_find(&s->windows,s->window[0]) || !visible(s,1))return 0;
    struct usr_process_row rows[2]={0};size_t count=0;
    if(usr_processes(&s->users,s->pid[1],rows,2,&count)!=USR_OK ||
       count!=1 || rows[0].pid!=s->pid[1] ||
       usr_processes(&s->users,s->pid[0],rows,2,&count)!=USR_OK ||
       count!=1 || rows[0].pid!=s->pid[0])return 0;
    return 1;
}
int nsh_on_event(struct nsh_stage *s,unsigned slot,enum ps_event event)
{
    if(!s || !s->ready || slot>=2 || !s->pid[slot])return 0;
    if(event==PS_YIELD){
        return live_pid(s,slot) && ws_snapshot(&s->windows,0,0)==2 &&
               visible(s,1);
    }
    if(event!=PS_FAULT && event!=PS_EXIT)return 0;
    if(pr_is_live(s->users.processes,s->pid[slot]) ||
       usr_reconcile(&s->users)!=1 ||
       ws_find(&s->windows,s->window[slot]))return 0;
    if(slot==0)
        return ws_snapshot(&s->windows,0,0)==1 && live_pid(s,1) && visible(s,1);
    return ws_snapshot(&s->windows,0,0)==0 &&
           usr_visible_windows(&s->users,0,0)==0;
}
int nsh_finish(struct nsh_stage *s)
{
    if(!s || !s->ready || pr_snapshot(s->users.processes,0,0,true) ||
       ws_snapshot(&s->windows,0,0) ||
       usr_lock(&s->users)!=USR_OK ||
       usr_close(&s->users,s->sid[0])!=USR_OK ||
       usr_close(&s->users,s->sid[1])!=USR_OK ||
       usr_find(&s->users,s->sid[0]) || usr_find(&s->users,s->sid[1]))return 0;
    s->ready=false;return 1;
}
