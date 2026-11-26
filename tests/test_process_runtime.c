#include <styleos/process_runtime.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static struct pr_table table;
static struct pr_entry list[PR_MAX_PROCESSES];
/* Each mutation must succeed at MAX-1, then refuse at MAX without
 * modifying any registry byte, output PID, or child parent link. */
static void serial_boundary(void)
{
    struct pr_table t, before;
    uint32_t parent=0, child=0, output=0xdeadbeef;
    struct pr_admission a={PR_NATIVE_USER,0,1,0x2000,4096,"parent"};
    pr_init(&t);
    t.change_serial=UINT32_MAX-1;
    assert(pr_admit(&t,&a,&parent)==PR_OK && t.change_serial==UINT32_MAX);
    a.parent_pid=parent; a.scheduler_handle=2; a.page_root=0x3000; a.label="child";
    memcpy(&before,&t,sizeof t);
    assert(pr_admit(&t,&a,&output)==PR_EXHAUSTED && output==0xdeadbeef);
    assert(memcmp(&before,&t,sizeof t)==0);
    t.change_serial=UINT32_MAX-1;
    assert(pr_transition(&t,parent,1,PR_RUNNING)==PR_OK && t.change_serial==UINT32_MAX);
    memcpy(&before,&t,sizeof t);
    assert(pr_transition(&t,parent,1,PR_WAITING)==PR_EXHAUSTED);
    assert(memcmp(&before,&t,sizeof t)==0);
    t.change_serial=UINT32_MAX-1;
    assert(pr_account(&t,parent,1,7,8192)==PR_OK && t.change_serial==UINT32_MAX);
    memcpy(&before,&t,sizeof t);
    assert(pr_account(&t,parent,1,1,4096)==PR_EXHAUSTED);
    assert(memcmp(&before,&t,sizeof t)==0);
    t.change_serial=0;
    assert(pr_admit(&t,&a,&child)==PR_OK);
    assert(pr_transition(&t,parent,1,PR_EXITED)==PR_OK);
    t.change_serial=UINT32_MAX;
    memcpy(&before,&t,sizeof t);
    assert(pr_reap(&t,parent,1)==PR_EXHAUSTED);
    assert(memcmp(&before,&t,sizeof t)==0 && pr_find(&t,child)->parent_pid==parent);
    t.change_serial=UINT32_MAX-1;
    assert(pr_reap(&t,parent,1)==PR_OK && t.change_serial==UINT32_MAX);
    assert(!pr_find(&t,parent) && pr_find(&t,child)->parent_pid==0);
    assert(pr_transition(&t,child,2,PR_RUNNING)==PR_EXHAUSTED);
    puts("PROCESS SERIAL BOUNDARY PASS: four mutators, no wrap or partial mutation");
}
int main(void)
{
    assert(pr_global() == pr_global());
    assert(pr_snapshot(pr_global(),list,PR_MAX_PROCESSES,false)==0);
    pr_init(&table);
    assert(table.next_pid == 1);
    assert(pr_snapshot(&table, list, PR_MAX_PROCESSES, false) == 0);
    struct pr_admission task = {PR_KERNEL_TASK,0,0xabc,0,4096,"desktop-runtime"};
    uint32_t kernel = 0, editor = 0, notes = 0;
    assert(pr_admit(&table,&task,&kernel) == PR_OK && kernel == 1);
    struct pr_admission app = {PR_NATIVE_USER,kernel,0xe01,0x2000,8192,"org.styleos.editor"};
    assert(pr_admit(&table,&app,&editor) == PR_OK && editor == 2);
    app.scheduler_handle = 0xe02;
    app.page_root = 0x3000;
    app.label = "org.styleos.notes";
    assert(pr_admit(&table,&app,&notes) == PR_OK && notes == 3);
    assert(pr_snapshot(&table,list,PR_MAX_PROCESSES,false) == 3);
    assert(list[0].type == PR_KERNEL_TASK && list[1].type == PR_NATIVE_USER);
    assert(list[1].parent_pid == kernel && list[2].parent_pid == kernel);
    assert(strcmp(list[1].label,"org.styleos.editor") == 0);
    assert(pr_transition(&table,editor,0xe01,PR_RUNNING) == PR_OK);
    assert(pr_account(&table,editor,0xe01,15,4096) == PR_OK);
    assert(pr_find(&table,editor)->cpu_ticks == 15);
    assert(pr_find(&table,editor)->resident_bytes == 4096);
    assert(pr_transition(&table,editor,0xe01,PR_WAITING) == PR_OK);
    assert(pr_account(&table,editor,0xfff,30,100) == PR_BAD_EVIDENCE);
    assert(pr_transition(&table,editor,0xfff,PR_EXITED) == PR_BAD_EVIDENCE);
    assert(pr_transition(&table,editor,0xe01,PR_FAULTED) == PR_OK);
    assert(pr_is_live(&table,notes) && !pr_is_live(&table,editor));
    assert(pr_snapshot(&table,list,PR_MAX_PROCESSES,false) == 2);
    assert(pr_snapshot(&table,list,PR_MAX_PROCESSES,true) == 3);
    assert(pr_reap(&table,editor,0xe01) == PR_OK);
    assert(pr_find(&table,editor) == NULL);
    assert(pr_snapshot(&table,list,PR_MAX_PROCESSES,true) == 2);
    assert(pr_transition(&table,notes,0xe02,PR_EXITED) == PR_OK);
    assert(pr_reap(&table,kernel,0xabc) == PR_INVALID_STATE);
    assert(pr_reap(&table,notes,0xe02) == PR_OK);
    task.scheduler_handle = 0xdef;
    uint32_t new_pid=0;
    assert(pr_admit(&table,&task,&new_pid)==PR_OK && new_pid==4);
    assert(pr_admit(&table,&task,&new_pid)==PR_CONFLICT);
    task.scheduler_handle=0;
    assert(pr_admit(&table,&task,&new_pid)==PR_BAD_EVIDENCE);
    task.scheduler_handle=0x1fff;
    task.label="";
    assert(pr_admit(&table,&task,&new_pid)==PR_BAD_ARGUMENT);
    assert(pr_is_live(&table,9999)==false);
    pr_init(&table);
    task.label="kernel";
    task.scheduler_handle=88;
    table.next_pid=UINT32_MAX;
    assert(pr_admit(&table,&task,&new_pid)==PR_EXHAUSTED);
    serial_boundary();
    puts("PROCESS REGISTRY PASS: real-context admission, kernel/user, lifecycle, metrics, no fake sessions");
    return 0;
}
