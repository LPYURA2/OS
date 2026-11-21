#include <styleos/process_inspector.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct pr_table processes;
static struct ws_service windows;
static struct pi_row rows[PR_MAX_PROCESSES];
static struct pi_snapshot_info info;
int main(void)
{
    pr_init(&processes);
    ws_init(&windows);
    assert(pi_collect(&processes,&windows,rows,PR_MAX_PROCESSES,&info)==PI_OK);
    assert(info.total==0 && !info.cpu_percentage_available);
    assert(pi_collect(&processes,&windows,0,1,&info)==PI_ARGUMENT);
    struct pr_admission k = {PR_KERNEL_TASK,0,0x81,0,8192,"StyleOS kernel"};
    uint32_t kernel,editor,service,child,modal;
    assert(pr_admit(&processes,&k,&kernel)==PR_OK);
    struct pr_admission e = {PR_NATIVE_USER,kernel,0x82,0x2000,16384,"org.styleos.editor"};
    assert(pr_admit(&processes,&e,&editor)==PR_OK);
    e.scheduler_handle=0x83;e.page_root=0x3000;e.label="background-helper";
    assert(pr_admit(&processes,&e,&service)==PR_OK);
    e.scheduler_handle=0x84;e.page_root=0x4000;e.parent_pid=editor;e.label="child-worker";
    assert(pr_admit(&processes,&e,&child)==PR_OK);
    struct ws_bounds rectangle={20,30,800,500};
    assert(ws_open_app(&windows,&processes,editor,"Редактор",rectangle,&modal)==WS_OK);
    uint32_t second;
    assert(ws_open_app(&windows,&processes,editor,"Документ",rectangle,&second)==WS_OK);
    /* Installer dialogs are owned by the system, not a synthetic process. */
    uint32_t dialog;
    assert(ws_open_package_dialog(&windows,"Установка приложения",WS_INSTALL_INCOMPATIBLE,
                                  rectangle,&dialog)==WS_OK);
    assert(pi_collect(&processes,&windows,rows,PR_MAX_PROCESSES,&info)==PI_OK);
    assert(info.total==4 && info.applications==1 && info.background==2 && info.system==1);
    assert(!info.cpu_percentage_available);
    assert(rows[0].pid==editor && rows[0].group==PI_APPLICATIONS && rows[0].open_windows==2);
    assert(rows[1].pid==service && rows[1].group==PI_BACKGROUND);
    assert(rows[2].pid==child && rows[2].parent_pid==editor);
    assert(rows[3].pid==kernel && rows[3].group==PI_SYSTEM);
    assert(pi_can_request_end(&processes,kernel)==PI_TARGET_PROTECTED);
    assert(pi_can_request_end(&processes,editor)==PI_TARGET_ELIGIBLE);
    assert(pi_can_request_end(&processes,99999)==PI_TARGET_MISSING);
    assert(pi_collect(&processes,&windows,rows,2,&info)==PI_TRUNCATED);
    assert(info.total==4 && info.applications==1 && info.background==2 && info.system==1);
    assert(ws_compositor_restart(&windows)==2);
    assert(pi_collect(&processes,&windows,rows,PR_MAX_PROCESSES,&info)==PI_OK);
    assert(info.window_generation==2 && rows[0].pid==editor);
    assert(pr_transition(&processes,editor,0x82,PR_FAULTED)==PR_OK);
    ws_reap_dead_owners(&windows,&processes);
    assert(pi_collect(&processes,&windows,rows,PR_MAX_PROCESSES,&info)==PI_OK);
    assert(info.applications==0 && info.background==2 && info.system==1);
    assert(pi_can_request_end(&processes,editor)==PI_TARGET_MISSING);
    assert(pr_account(&processes,service,0x83,21,30000)==PR_OK);
    assert(pi_collect(&processes,&windows,rows,PR_MAX_PROCESSES,&info)==PI_OK);
    assert(rows[0].pid==service && rows[0].cpu_ticks==21 &&
           rows[0].resident_bytes==30000 && !info.cpu_percentage_available);
    puts("PROCESS INSPECTOR PASS: Task Manager groups, real PIDs, parent tree, F10, protections");
    return 0;
}
