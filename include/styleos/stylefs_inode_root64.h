
#ifndef STYLEOS_INODE_ROOT64_H
#define STYLEOS_INODE_ROOT64_H
#include <styleos/stylefs_inode_pages64.h>
/* Two root sectors followed by two equal page-aligned metadata banks.
 * Separate candidate format, exclusively reserved metadata slice.
 * Caller serializes and proves that referenced data are durable and
 * protected across BOTH retained snapshots. No file allocation here.
 * No raw writer or callback reentry; cold reopen after uncertainty.
 */
struct ir64_state {
 struct bs_service *block;
 uint64_t owner,token,sectors;
 unsigned active;
 bool valid;
 struct ip64_root root;
};
enum ip64_status ir64_mount(struct ir64_state *,struct bs_service *,uint64_t owner,uint64_t token,
 uint64_t sectors,uint64_t data_capacity,uint64_t data_start,struct ip64_workspace *,
 struct sfs64_run *,size_t cap);
enum ip64_status ir64_mount_epoch(struct ir64_state *,struct bs_service *,uint64_t owner,uint64_t token,
 uint64_t sectors,uint64_t data_capacity,uint64_t data_start,uint64_t epoch,struct ip64_workspace *,
 struct sfs64_run *,size_t cap);
/* Full immutable snapshot rebuild in inactive bank. Validate all new pages
 * and extent ownership within proposed snapshot before publishing root.
 * Not incremental COW. Old selected snapshot untouched on cancellation.
 */
enum ip64_status ir64_publish(struct ir64_state *,struct ip64_workspace *,struct sfs64_run *,size_t cap,
 uint64_t count,ip64_source,void *);
#ifdef STYLEFS_HOST_FORMAT
/* Explicitly reserved blank slice only. Checks both root sectors. */
enum ip64_status ir64_provision(struct bs_service *,uint64_t owner,uint64_t token,
 uint64_t sectors,uint64_t data_capacity,uint64_t data_start);
#endif
#endif
