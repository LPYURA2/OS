#ifndef STYLEOS_PAGED_PAIR64_H
#define STYLEOS_PAGED_PAIR64_H
#include <styleos/stylefs_dircommit64.h>
#include <styleos/stylefs_inode_root64.h>
/* Separate candidate coordinated paged format. Four disjoint slices on
 * one device; data extents are relative to the data grant, not inode
 * metadata. Trusted serialized supervisor owns all staging and protects
 * both retained snapshots' extents. At most one unpublished epoch per
 * component. No raw restaging after uncertainty. Read-only recovery;
 * guarded data allocation and rollback are separate future work. */
enum pp64_status { PP64_OK,PP64_INVALID,PP64_CORRUPT,PP64_IO,PP64_FULL,PP64_UNCERTAIN };
struct pp64_layout {uint64_t owner,receipt,dir,inode,data,dir_sectors,inode_sectors,data_sectors,data_start;};
struct pp64_pair {uint64_t sequence,dir_epoch,inode_epoch;unsigned slot;};
struct pp64_workspace {
 struct dc64_state dir;
 struct dc64_edit_workspace dw;
 struct ir64_state inode;
 struct ip64_workspace iw;
};
enum pp64_status pp64_mount(struct bs_service *,const struct pp64_layout *,struct pp64_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *,size_t run_cap,struct pp64_pair *);
enum pp64_status pp64_commit(struct bs_service *,const struct pp64_layout *,struct pp64_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *,size_t run_cap,
 uint64_t expected_sequence,uint64_t dir_epoch,uint64_t inode_epoch,struct pp64_pair *);
#ifdef STYLEFS_HOST_FORMAT
enum pp64_status pp64_provision(struct bs_service *,const struct pp64_layout *,struct pp64_workspace *,
 struct dc64_node *,size_t node_cap,struct sfs64_run *,size_t run_cap,struct pp64_pair *);
#endif
#endif
