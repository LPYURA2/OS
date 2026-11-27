#ifndef STYLEOS_FILE_SYSCALL43_H
#define STYLEOS_FILE_SYSCALL43_H
#include <styleos/runtime_bank_862.h>
#include <styleos/stylefs_process_file64.h>
#include <styleos/user_copy_772.h>
/* Candidate safe-root syscall adapter, compiled only in its host profile.
 * GPR indices follow raw frame order: [0] call, [1] token, [2] file offset,
 * [3] destination VA, [4] byte count. Result [0] count or negative WV64 status.
 * At most 32 bytes within S846_DATA, reuse existing range validator.
 * Architecture must supply authenticated CPU receipt, pinned page and
 * non-mutating audit that proves actual user writable/NX mapping,
 * safe-root backing and current lease/owner. Neither audit nor page
 * pointer comes from user space. No hardware authentication in host tests. */
enum { FS43_CALL_READ=0x40 };
struct fs43_binding {
 const struct pf64_service *service;
 struct vm64_mount *mount;
 struct wv64_workspace *work;
 uint8_t *page;
 bool (*audit)(void *,const struct rt862_selection *,const uint8_t *);
 void *context;
};
#ifdef STYLEOS_TEST_FILE_SYSCALL_43
enum ps_status rt43_file_read(struct rt862_bank *,struct dpr_service *,
 const struct rt862_selection *,uint64_t,uint64_t,const struct s847_raw *,uint64_t,uint64_t,const struct fs43_binding *);
#endif
#endif
