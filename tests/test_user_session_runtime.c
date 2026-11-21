#include <styleos/user_session_runtime.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct pr_table processes;
static struct ws_service windows;
static struct usr_runtime service;
static struct usr_process_row rows[PR_MAX_PROCESSES];
static struct ws_window winrows[WS_MAX_WINDOWS];
static uint32_t add(enum pr_type type,uint64_t handle,uint32_t parent,
                    const char *name)
{
    const struct pr_admission s = {type,parent,handle,
                                  type==PR_NATIVE_USER ? handle*4096u:0,0,name};
    uint32_t pid=0;
    assert(pr_admit(&processes,&s,&pid)==PR_OK);
    return pid;
}
int main(void)
{
    pr_init(&processes); ws_init(&windows);
    assert(usr_global()==0);
    assert(usr_init(&service,&processes,&windows)==USR_OK);
    assert(usr_init(&service,&processes,&windows)==USR_BUSY);
    assert(!service.selected_session && service.next_session==1);
    uint32_t alice=0,bob=0;
    assert(usr_create(&service,0,&alice)==USR_INVALID);
    assert(usr_create(&service,12,&alice)==USR_OK && alice==1);
    assert(usr_create(&service,12,&bob)==USR_BUSY);
    assert(usr_create(&service,34,&bob)==USR_OK && bob==2);
    assert(usr_select(&service,alice)==USR_OK);
    assert(usr_find(&service,alice)->state==USR_ACTIVE);
    uint32_t k=add(PR_KERNEL_TASK,9,0,"Scheduler");
    uint32_t a=add(PR_NATIVE_USER,10,0,"Editor.tea");
    uint32_t b=add(PR_NATIVE_USER,11,0,"Notes.tea");
    uint32_t child=add(PR_NATIVE_USER,12,a,"AppChild.tea");
    assert(usr_bind(&service,alice,k,9)==USR_DENIED);
    assert(usr_bind(&service,alice,a,99)==USR_DENIED);
    assert(usr_bind(&service,bob,a,10)==USR_DENIED);
    assert(usr_bind(&service,alice,a,10)==USR_OK);
    assert(usr_bind(&service,alice,a,10)==USR_BUSY);
    assert(usr_bind(&service,alice,child,12)==USR_OK);
    assert(usr_bind(&service,alice,b,11)==USR_OK);
    uint32_t win_a=0, win_b=0;
    struct ws_bounds rect={0,0,350,240};
    assert(ws_open_app(&windows,&processes,a,"Editor",rect,&win_a)==WS_OK);
    assert(ws_open_app(&windows,&processes,b,"Notes",rect,&win_b)==WS_OK);
    uint32_t install=0;
    assert(ws_open_package_dialog(&windows,"Install",WS_INSTALL_REVIEWED,rect,&install)==WS_OK);
    assert(usr_visible_windows(&service,winrows,WS_MAX_WINDOWS)==2);
    assert(usr_visible_windows(&service,winrows,1)==2 && winrows[0].id==win_a);
    size_t total=99;
    assert(usr_processes(&service,0,rows,PR_MAX_PROCESSES,&total)==USR_INVALID);
    assert(usr_processes(&service,k,rows,PR_MAX_PROCESSES,&total)==USR_DENIED);
    assert(usr_processes(&service,a,rows,PR_MAX_PROCESSES,&total)==USR_OK);
    assert(total==3 && rows[0].pid==a && rows[1].pid==child && rows[2].pid==b);
    assert(rows[0].state==PR_READY && !strcmp(rows[0].label,"Editor.tea"));
    assert(usr_select(&service,bob)==USR_OK);
    assert(usr_find(&service,alice)->state==USR_LOCKED);
    assert(usr_visible_windows(&service,winrows,WS_MAX_WINDOWS)==0);
    assert(ws_compositor_restart(&windows)==2);
    uint32_t b2=add(PR_NATIVE_USER,13,0,"Documents.tea");
    assert(usr_bind(&service,bob,b2,13)==USR_OK);
    uint32_t wb2=0;
    assert(ws_open_app(&windows,&processes,b2,"Documents",rect,&wb2)==WS_OK);
    assert(usr_visible_windows(&service,winrows,WS_MAX_WINDOWS)==1 && winrows[0].id==wb2);
    assert(usr_processes(&service,b2,rows,PR_MAX_PROCESSES,&total)==USR_OK);
    assert(total==1 && rows[0].pid==b2);
    assert(usr_processes(&service,a,rows,PR_MAX_PROCESSES,&total)==USR_OK);
    assert(total==3 && rows[0].pid==a);
    assert(usr_bind(&service,bob,child,12)==USR_BUSY);
    uint32_t cross=add(PR_NATIVE_USER,14,a,"Cross.tea");
    assert(usr_bind(&service,bob,cross,14)==USR_DENIED);
    assert(usr_close(&service,bob)==USR_BUSY);
    assert(usr_lock(&service)==USR_OK);
    assert(usr_visible_windows(&service,winrows,WS_MAX_WINDOWS)==0);
    assert(usr_close(&service,bob)==USR_BUSY); /* live member */
    assert(usr_select(&service,alice)==USR_OK);
    assert(usr_visible_windows(&service,winrows,WS_MAX_WINDOWS)==2);
    assert(pr_transition(&processes,a,10,PR_EXITED)==PR_OK);
    assert(usr_reconcile(&service)==1);
    assert(ws_find(&windows,win_a)==0 && ws_find(&windows,win_b)!=0);
    assert(usr_processes(&service,a,rows,PR_MAX_PROCESSES,&total)==USR_DENIED);
    assert(usr_processes(&service,child,rows,PR_MAX_PROCESSES,&total)==USR_OK && total==2);
    assert(ws_find(&windows,install)!=0); /* kernel-owned modal, never shown to apps */
    assert(pr_transition(&processes,child,12,PR_EXITED)==PR_OK);
    assert(pr_transition(&processes,b,11,PR_FAULTED)==PR_OK);
    assert(usr_reconcile(&service)==2 && !ws_find(&windows,win_b));
    assert(pr_transition(&processes,b2,13,PR_EXITED)==PR_OK);
    assert(usr_reconcile(&service)==1);
    assert(usr_lock(&service)==USR_OK);
    assert(usr_close(&service,bob)==USR_OK);
    assert(usr_find(&service,bob)==0);
    assert(usr_close(&service,alice)==USR_OK);
    uint32_t newer=0;
    assert(usr_create(&service,55,&newer)==USR_OK && newer>bob);
    puts("USER SESSION RUNTIME PASS: independent app owners, isolated windows, real PID, lock, switch, deny, reap");
    return 0;
}
