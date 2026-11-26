#ifndef STYLEOS_RECOVERY_INTERNAL_H
#define STYLEOS_RECOVERY_INTERNAL_H
#include <styleos/stylefs_dircommit64.h>
/* Internal only. Selected state may be older than newest on disk.
 * Do not expose or mutate it until coordinator invalidates newer receipt. */
enum dc64_status dc64_repair_select(struct dc64_state *,struct bs_service *,uint64_t owner,
 uint64_t token,uint64_t sectors,uint64_t wanted,struct dc64_workspace *,int *discard);
#include <styleos/stylefs_inodecommit64.h>
enum ic64_status ic64_repair_select(struct ic64_state *,struct bs_service *,uint64_t owner,
 uint64_t token,uint64_t sectors,uint64_t capacity,uint64_t data_start,uint64_t wanted,
 struct ic64_workspace *,int *discard);
#endif
