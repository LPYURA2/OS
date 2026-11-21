#include <styleos/window_service.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct pr_table processes;
static struct ws_service windows;
static struct ws_window visible[WS_MAX_WINDOWS];
int main(void)
{
    assert(ws_global() == ws_global());
    assert(ws_snapshot(ws_global(),visible,WS_MAX_WINDOWS)==0);
    pr_init(&processes);ws_init(&windows);
    struct ws_bounds rect={130,80,840,570};
    uint32_t denied=0,editor=0,notes=0,app_window=0,modal=0;
    assert(ws_snapshot(&windows,visible,WS_MAX_WINDOWS)==0);
    assert(ws_open_app(&windows,&processes,42,"Поддельный",rect,&denied)==WS_DENIED);
    struct pr_admission kernel={PR_KERNEL_TASK,0,101,0,4096,"kernel"};
    assert(pr_admit(&processes,&kernel,&denied)==PR_OK);
    assert(ws_open_app(&windows,&processes,denied,"kernel",rect,&app_window)==WS_DENIED);
    struct pr_admission spec={PR_NATIVE_USER,denied,102,4096,8192,"org.styleos.editor"};
    assert(pr_admit(&processes,&spec,&editor)==PR_OK);
    spec.scheduler_handle=103;spec.page_root=8192;spec.label="org.styleos.notes";
    assert(pr_admit(&processes,&spec,&notes)==PR_OK);
    assert(ws_open_app(&windows,&processes,editor,"Текстовый редактор",rect,&app_window)==WS_OK);
    assert(ws_find(&windows,app_window)->owner_pid==editor);
    assert(ws_set_minimized(&windows,notes,app_window,true)==WS_DENIED);
    assert(ws_close_app(&windows,notes,app_window)==WS_DENIED);
    assert(ws_set_minimized(&windows,editor,app_window,true)==WS_OK);
    assert(ws_find(&windows,app_window)->minimized);
    assert(ws_compositor_restart(&windows)==2);
    assert(ws_find(&windows,app_window)->minimized);
    assert(ws_snapshot(&windows,visible,WS_MAX_WINDOWS)==1);
    /* Approved system-dialog contract: compatible package or refusal. */
    assert(ws_open_package_dialog(&windows,"Установка приложения",WS_INSTALL_INCOMPATIBLE,rect,&modal)==WS_OK);
    assert(ws_find(&windows,modal)->install_enabled==false);
    assert(strcmp(ws_find(&windows,modal)->error_code,"TEA_INCOMPATIBLE")==0);
    assert(ws_close_app(&windows,editor,modal)==WS_DENIED);
    assert(ws_close_system(&windows,app_window)==WS_DENIED);
    assert(ws_close_system(&windows,modal)==WS_OK);
    assert(ws_open_package_dialog(&windows,"Установка приложения",WS_INSTALL_REVIEWED,rect,&modal)==WS_OK);
    assert(ws_find(&windows,modal)->install_enabled==true);
    assert(ws_find(&windows,modal)->error_code==NULL);
    assert(ws_open_package_dialog(&windows,"Недопустимый",0,rect,&modal)==WS_INVALID);
    assert(pr_transition(&processes,editor,102,PR_FAULTED)==PR_OK);
    ws_reap_dead_owners(&windows,&processes);
    assert(!ws_find(&windows,app_window));
    assert(ws_find(&windows,modal));
    assert(ws_close_system(&windows,modal)==WS_OK);
    assert(ws_snapshot(&windows,visible,WS_MAX_WINDOWS)==0);
    puts("WINDOW SERVICE PASS: owner-auth, F10 persistence, dialogs, incompatible install disabled");
    return 0;
}
