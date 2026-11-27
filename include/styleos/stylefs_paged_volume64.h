#ifndef STYLEOS_PAGED_VOLUME64_H
#define STYLEOS_PAGED_VOLUME64_H
#include <styleos/stylefs_paged_object64.h>
/* Persistent kernel volume owner. Explicit geometry and authenticated
 * block generation only; no auto-probe or formatting. Zero-init once,
 * never copy or reset live state. Supervisor serializes all access,
 * including object calls and mount/unmount. All spans are trusted.
 * Mount owns four grants until close, including after broker failure.
 * Caller retains device and workspace lifetime. No CPL3 pointers. */
struct pv64_geometry {
 uint64_t start,sectors,dir_sectors,inode_sectors,data_start;
};
struct pv64_volume {
 struct po64_service objects;
 struct bs_service *block;
 struct pp64_layout layout;
 uint64_t generation;
 bool attached;
};
enum po64_status pv64_mount(struct pv64_volume *,struct bs_service *,uint64_t generation,uint64_t owner,
 const struct pv64_geometry *,bool writable,struct pp64_stream_workspace *,struct dc64_node *,size_t,
 struct sfs64_run *,struct sfs64_run *,size_t);
/* Revoke exactly the owned grants, not all grants of the principal.
 * Idempotent after media removal. Refuse while callback is in flight.
 * No implicit retry after uncertain write; supervisor must recover device. */
enum po64_status pv64_close(struct pv64_volume *);
#endif
