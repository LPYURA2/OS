#include <styleos/process_view.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct pr_table p;
static struct ws_service w;
static struct pi_row r[PR_MAX_PROCESSES];
static struct pi_view_info info;
static uint32_t add(enum pr_type t,uint32_t par,uint64_t handle,
                    uint64_t root,uint64_t mem,const char *name)
{
    struct pr_admission in={t,par,handle,root,mem,name};
    uint32_t pid=0;assert(pr_admit(&p,&in,&pid)==PR_OK);return pid;
}
static void query(enum pi_order by,int desc,const char *term)
{
    struct pi_query q={by,!!desc,term};
    assert(pi_view(&p,&w,&q,r,PR_MAX_PROCESSES,&info)==PI_OK);
}
int main(void)
{
    pr_init(&p);ws_init(&w);
    uint32_t k=add(PR_KERNEL_TASK,0,1,0,1000,"StyleOS Kernel");
    uint32_t a=add(PR_NATIVE_USER,k,2,0x2000,4000,"Editor");
    uint32_t b=add(PR_NATIVE_USER,k,3,0x3000,6000,"Browser");
    uint32_t c=add(PR_NATIVE_USER,b,4,0x4000,2000,"background helper");
    struct ws_bounds rect={1,2,200,100};uint32_t win;
    assert(ws_open_app(&w,&p,a,"Editor",rect,&win)==WS_OK);
    assert(ws_open_app(&w,&p,b,"Browser",rect,&win)==WS_OK);
    assert(pr_account(&p,a,2,20,4000)==PR_OK);
    assert(pr_account(&p,b,3,60,6000)==PR_OK);
    assert(pr_account(&p,c,4,10,2000)==PR_OK);
    query(PI_BY_PID,0,0);
    assert(info.source.total==4 && info.matched==4);
    assert(info.source.applications==2 && info.source.background==1 &&
           info.source.system==1 && !info.source.cpu_percentage_available);
    assert(r[0].pid==a && r[1].pid==b && r[2].pid==c && r[3].pid==k);
    query(PI_BY_CPU_TICKS,1,"");
    assert(r[0].pid==b && r[1].pid==a && r[2].pid==c);
    query(PI_BY_MEMORY,0,"");
    assert(r[0].pid==a && r[1].pid==b);
    query(PI_BY_NAME,0,0);
    assert(r[0].pid==b && r[1].pid==a);
    query(PI_BY_PID,0,"EDITOR");
    assert(info.matched==1 && r[0].pid==a);
    query(PI_BY_PID,0,"3");
    assert(info.matched==1 && r[0].pid==b);
    query(PI_BY_PID,0,"unmatched");
    assert(info.matched==0 && info.source.total==4);
    struct pi_query q={PI_BY_PID,false,0};
    assert(pi_view(&p,&w,&q,r,1,&info)==PI_TRUNCATED &&
           info.matched==4 && r[0].pid==a);
    assert(pi_view(&p,&w,&q,0,0,&info)==PI_TRUNCATED);
    assert(pi_view(&p,&w,&q,0,1,&info)==PI_ARGUMENT);
    q.by=(enum pi_order)0;
    assert(pi_view(&p,&w,&q,r,PR_MAX_PROCESSES,&info)==PI_ARGUMENT);
    q.by=PI_BY_PID;
    char invalid[PI_MAX_SEARCH+1];
    memset(invalid,'X',sizeof invalid);invalid[PI_MAX_SEARCH]=0;
    q.search=invalid;
    assert(pi_view(&p,&w,&q,r,PR_MAX_PROCESSES,&info)==PI_ARGUMENT);
    assert(ws_compositor_restart(&w)==2);
    query(PI_BY_PID,0,0);
    assert(info.source.window_generation==2 && r[0].pid==a);
    assert(pr_transition(&p,b,3,PR_FAULTED)==PR_OK);
    ws_reap_dead_owners(&w,&p);
    query(PI_BY_PID,0,0);
    assert(info.matched==3 && info.source.applications==1 &&
           r[0].pid==a && r[1].pid==c && r[2].pid==k);
    puts("TASK MANAGER VIEW PASS: real PIDs, groups, search, sort, limits, F10");
    return 0;
}
