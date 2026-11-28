#ifndef STYLEOS_EXPLORER_VOLUME_115_H
#define STYLEOS_EXPLORER_VOLUME_115_H
#include <styleos/stylefs_paged_volume64.h>
/* Trusted supervisor supplies ONLY volumes visible to the selected session.
 * Borrowed pointers and all output spans are trusted and disjoint.
 * One block service per enumeration; no raw pointers in exported record.
 * No implied permission to open a volume, and no free-space estimate. */
enum { EX115_VOLUME_MAX=4 };
struct ex115_volume_info {
 uint64_t device_generation,mount,sequence,start_sector,volume_bytes,device_bytes;
 bool writable;
};
enum po64_status ex115_volume_info(struct pv64_volume *,struct ex115_volume_info *);
/* Stable ordered list of explicitly supplied candidates. One candidate per
 * step; stale/unattached is NOT an empty directory. Caller starts fresh
 * after topology changes. No probing, mounting, or device names invented. */
enum po64_status ex115_volume_at(struct bs_service *,struct pv64_volume *const *,size_t,size_t,struct ex115_volume_info *);
/* Fixed little-endian record: version/flags, mount/sequence, volume bytes,
 * visible block span bytes (NOT necessarily physical disk), free data bytes,
 * largest free run bytes, reserved zero. Exact authorized snapshot. */
enum { EX117_VOLUME_BYTES=64 };
enum po64_status ex117_volume_wire(struct pv64_volume *,struct po64_snapshot,
 struct sfs64_run *,uint8_t *,size_t);
#endif
