#ifndef STYLEOS_VOLUME_SERVICE_H
#define STYLEOS_VOLUME_SERVICE_H
#include <styleos/block_service.h>
#include <styleos/stylefs.h>
/* Kernel-only, serialized synchronous service. Zero-init once; do not copy
 * or reset a live service. All pointers/callbacks are trusted kernel spans.
 * Volume and entry references are NOT user capabilities; authenticated file
 * handles and access control belong to a later broker. */
enum { SV_SLOTS=2 };
enum sv_status { SV_OK, SV_INVALID, SV_DENIED, SV_STALE, SV_BUSY, SV_FULL,
                 SV_RANGE, SV_IO, SV_UNCERTAIN, SV_RECOVERY, SV_SIGNATURE,
                 SV_CORRUPT, SV_NOT_FOUND, SV_EXISTS, SV_TYPE, SV_NAME };
struct sv_ref { uint64_t mount; uint32_t revision; unsigned index; };
struct sv_info { uint64_t mount; uint32_t sectors,revision; unsigned format; bool writable; };
struct sv_slot {
    struct bs_service *block;
    uint64_t owner,token,mount;
    uint32_t sectors;
    bool writable,active;
    enum bs_status last_block;
    struct sfs_volume fs;
    struct sfs_workspace work;
};
struct sv_service {
    uint64_t serial;
    bool busy;
    struct sv_slot slots[SV_SLOTS];
};
/* Attach a bounded existing volume. Mount never writes or formats.
 * Supervisor supplies authenticated physical generation and principal.
 * Failed mounts release their grant; a quarantined device remains so. */
enum sv_status sv_mount(struct sv_service *,struct bs_service *,uint64_t generation,
                        uint64_t owner,uint64_t start,uint32_t sectors,bool writable,uint64_t *mount);
enum sv_status sv_unmount(struct sv_service *,uint64_t mount);
enum sv_status sv_info(struct sv_service *,uint64_t mount,struct sv_info *);
enum sv_status sv_root(struct sv_service *,uint64_t mount,struct sv_ref *);
enum sv_status sv_lookup(struct sv_service *,struct sv_ref parent,const char *name,struct sv_ref *);
enum sv_status sv_stat(struct sv_service *,struct sv_ref,struct sfs_entry *);
/* Iterate actual children by entry index, from 1 through SFS_ENTRIES-1.
 * Returns next child and its metadata, or SV_NOT_FOUND. */
enum sv_status sv_next(struct sv_service *,struct sv_ref parent,unsigned after,struct sv_ref *,struct sfs_entry *);
enum sv_status sv_mkdir(struct sv_service *,struct sv_ref parent,const char *name,struct sv_ref *);
enum sv_status sv_create(struct sv_service *,struct sv_ref parent,const char *name,const uint8_t *,size_t,struct sv_ref *);
/* Sink bytes provisional until SV_OK (CRC); do not publish partial output.
 * Current v1/v2 size limits unchanged. No arbitrary userspace callbacks. */
enum sv_status sv_read_to(struct sv_service *,struct sv_ref,sfs_sink,void *);
#endif
