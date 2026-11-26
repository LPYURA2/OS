#ifndef STYLEOS_VOLUME_SERVICE_H
#define STYLEOS_VOLUME_SERVICE_H
#include <styleos/block_service.h>
#include <styleos/stylefs.h>
/* Kernel-only, serialized synchronous service. Zero-init once; do not copy
 * or reset a live service. All pointers/callbacks are trusted kernel spans.
 * Volume and entry references are NOT user capabilities; authenticated file
 * handles and access control belong to a later broker. */
enum { SV_SLOTS=2, SV_SCOPES=8 };
enum { SV_SCOPE_READ=1, SV_SCOPE_WRITE=2 };
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
/* Supervisor-owned capability records, not caller-supplied rights.
 * Each token is bound to a mount, metadata revision and authenticated
 * principal. Ordinary scopes can be rooted only in Users/<profile>. */
struct sv_scope {
    uint64_t token,owner,mount;
    uint32_t revision;
    unsigned root,rights;
};
struct sv_service {
    uint64_t serial;
    bool busy;
    struct sv_slot slots[SV_SLOTS];
    struct sv_scope scopes[SV_SCOPES];
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
/* Grant/revoke are supervisor-only. The supplied owner must be derived from
 * the authenticated caller by the future syscall broker, never from CPL3.
 * No scope can include the system root or other users' directories.
 * A successful own mutation advances that scope; all other scopes on the
 * same volume become stale until trusted re-authorization. */
enum sv_status sv_scope_grant(struct sv_service *,uint64_t owner,struct sv_ref profile,unsigned rights,uint64_t *token);
enum sv_status sv_scope_revoke(struct sv_service *,uint64_t owner,uint64_t token);
enum sv_status sv_scope_revoke_owner(struct sv_service *,uint64_t owner);
enum sv_status sv_scope_root(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref *);
enum sv_status sv_scope_lookup(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref,const char *,struct sv_ref *);
enum sv_status sv_scope_next(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref,unsigned,struct sv_ref *,struct sfs_entry *);
enum sv_status sv_scope_stat(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref,struct sfs_entry *);
enum sv_status sv_scope_read_to(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref,sfs_sink,void *);
enum sv_status sv_scope_mkdir(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref,const char *,struct sv_ref *);
enum sv_status sv_scope_create(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref,const char *,const uint8_t *,size_t,struct sv_ref *);
enum sv_status sv_scope_write(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref,const uint8_t *,size_t,struct sv_ref *);
enum sv_status sv_scope_move(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref,struct sv_ref,const char *,struct sv_ref *);
enum sv_status sv_scope_copy(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref,struct sv_ref,const char *,struct sv_ref *);
enum sv_status sv_scope_remove(struct sv_service *,uint64_t owner,uint64_t token,struct sv_ref);
#endif
