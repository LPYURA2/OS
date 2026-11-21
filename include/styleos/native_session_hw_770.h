#ifndef STYLEOS_NATIVE_SESSION_HW_770_H
#define STYLEOS_NATIVE_SESSION_HW_770_H

#include <styleos/process_runtime.h>
#include <styleos/process_scheduler.h>
#include <styleos/user_session_runtime.h>
#include <styleos/window_service.h>

/* Dedicated, controlled x86 CPL3 probe only. Two verified hardware-backed
 * contexts have already been admitted to the real process registry; this
 * associates their PIDs and roots with isolated test sessions and windows.
 * Not a login, filesystem ACL, desktop compositor or production launcher. */
struct nsh_stage {
    struct ws_service windows;
    struct usr_runtime users;
    uint32_t sid[2], pid[2], window[2];
    uint64_t handle[2], user_root[2];
    bool ready;
};
int nsh_init(struct nsh_stage *, struct pr_table *);
int nsh_attach(struct nsh_stage *, unsigned slot, uint32_t pid,
               uint64_t hardware_handle, uint64_t observed_user_root);
int nsh_review(struct nsh_stage *);
int nsh_on_event(struct nsh_stage *, unsigned slot, enum ps_event);
int nsh_finish(struct nsh_stage *);
#endif
