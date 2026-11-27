#ifndef STYLEOS_VOLUMEMOUNT64_H
#define STYLEOS_VOLUMEMOUNT64_H
#include <styleos/stylefs_widevolume64.h>
/* Supervisor-only lifetime adapter. Explicit admitted device, partition
 * geometry and lifetime-unique owner supplied by trusted storage layer.
 * No discovery/format, no auto-repair, no ownership of device attachment.
 * Zero-init once, never copy, serialize calls, stable caller buffers.
 * The four grants belong to this mount, not the desktop or applications.
 * Access to the embedded volume is trusted-only until a scoped broker exists. */
struct vm64_geometry {
 uint64_t generation,owner;
 uint64_t receipt_start,dir_start,inode_start,data_start;
 uint64_t dir_sectors,inode_sectors,data_sectors,data_reserved;
};
struct vm64_mount {
 struct bs_service *block;
 struct wv64_state volume;
 struct wv64_layout grants;
 bool active;
};
enum wv64_status vm64_open(struct vm64_mount *,struct bs_service *,const struct vm64_geometry *,
 bool write,struct ns64_node *,size_t,struct wv64_workspace *,struct ow64_inventory banks[2]);
/* All four exact tokens revoked, other clients untouched. A failed or
 * quarantined volume is closable; never detach a shared device. */
enum wv64_status vm64_close(struct vm64_mount *);
#endif
