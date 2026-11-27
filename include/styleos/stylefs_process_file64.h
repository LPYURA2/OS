#ifndef STYLEOS_PROCESS_FILE64_H
#define STYLEOS_PROCESS_FILE64_H
#include <styleos/process_runtime.h>
#include <styleos/stylefs_filebroker64.h>
/* One dedicated broker per stable registry lifetime. Registry PIDs never
 * repeat within that lifetime; neither registry nor broker may be reset
 * or copied while this service exists. Serialized trusted kernel entry.
 * Caller evidence comes from the scheduler/trap adapter, not request data.
 * No direct CPL3 buffers or syscall dispatch in this layer. */
struct pf64_service {const struct pr_table *processes;struct fbk64_broker *broker;};
struct pf64_identity {uint32_t pid;uint64_t handle,root;};
/* Supervisor grant after explicit selection; target may be waiting. */
enum wv64_status pf64_issue(const struct pf64_service *,struct vm64_mount *,struct wv64_workspace *,
 const struct pf64_identity *,const uint8_t *,size_t,uint64_t *);
/* Requesting process must be running, match authenticated handle/root.
 * Output is kernel staging only. */
enum wv64_status pf64_read(const struct pf64_service *,struct vm64_mount *,struct wv64_workspace *,
 const struct pf64_identity *,uint64_t token,uint64_t off,uint8_t *,size_t,size_t *);
/* Reclaim terminal/reaped process grants. Call at serialized exit/fault
 * boundaries; issue also sweeps. Reads deny dead principals independently. */
enum wv64_status pf64_sweep(const struct pf64_service *);
/* Explicit save permission issued by supervisor; authenticated running
 * process can replace only the selected revision using bounded kernel data. */
enum wv64_status pf64_issue_save(const struct pf64_service *,struct vm64_mount *,struct wv64_workspace *,
 const struct pf64_identity *,const uint8_t *,size_t,uint64_t *);
enum wv64_status pf64_save(const struct pf64_service *,struct vm64_mount *,struct wv64_workspace *,
 struct ow64_inventory banks[2],const struct pf64_identity *,uint64_t,const uint8_t *,size_t);
#endif
