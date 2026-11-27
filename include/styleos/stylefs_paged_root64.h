
#ifndef STYLEOS_PAGED_ROOT64_H
#define STYLEOS_PAGED_ROOT64_H
#include <styleos/stylefs_inode_pages64.h>
#include <styleos/stylefs_dircommit64.h>
/* Experimental paired directory + paged inode root. One dedicated metadata
 * slice: two receipt sectors, two equally sized banks of immutable pages.
 * Separate directory and data slices, all on same authenticated device.
 * No actual file data writes or allocator here. Trusted caller must prove
 * candidate extents durable and preserve the selected snapshot's contents.
 * Serialized only; raw directory writer may stage at most one new epoch.
 * On uncertainty stop, cold remount. No automatic format. */
enum pr64_status { PR64_OK,PR64_INVALID,PR64_CORRUPT,PR64_IO,PR64_FULL,PR64_CANCELLED,PR64_UNCERTAIN };
struct pr64_layout {uint64_t owner,meta,dir,data,meta_sectors,dir_sectors,data_sectors,data_start;};
struct pr64_view {uint64_t sequence,dir_epoch;unsigned slot;struct ip64_root inode;};
struct pr64_workspace {struct dc64_state dir;struct dc64_edit_workspace dw;struct ip64_workspace iw;};
/* Caller supplies disjoint graph/extent scratch. On success the selected
 * directory and inode root are immutable views, not capabilities. */
enum pr64_status pr64_mount(struct bs_service *,const struct pr64_layout *,
 struct pr64_workspace *,struct dc64_node *,size_t,struct sfs64_run *,size_t,struct pr64_view *);
/* Rebuild candidate maps in inactive bank, verify complete hierarchy and
 * bijection, publish one root receipt last. Old selected pair survives any
 * interrupted publication; the older fallback may be retired. */
enum pr64_status pr64_publish(struct bs_service *,const struct pr64_layout *,
 struct pr64_workspace *,struct dc64_node *,size_t,struct sfs64_run *,size_t,
 uint64_t expected_sequence,uint64_t dir_epoch,uint64_t files,ip64_source,void *,struct pr64_view *);
#ifdef STYLEFS_HOST_FORMAT
enum pr64_status pr64_provision(struct bs_service *,const struct pr64_layout *,
 struct pr64_workspace *,struct dc64_node *,size_t,struct sfs64_run *,size_t,struct pr64_view *);
#endif
#endif
