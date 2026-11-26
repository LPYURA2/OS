#ifndef STYLEOS_JOURNAL64_H
#define STYLEOS_JOURNAL64_H
#include <styleos/stylefs_map64.h>

/* Separate candidate wide metadata journal, not the production StyleFS
 * format. Sector 0 is the immutable geometry marker; each bank consists
 * of one commit header and eight payload sectors. Snapshot payload is
 * a sequence of LE u32 length + SF64MAP1 manifest; empty snapshot is valid.
 * No filenames, directory tree, data allocation or file data writes here.
 * Caller must hold the exclusive trusted block grant, serialize operations
 * and prove that all referenced data are durable, checksum-verified and
 * allocated without modifying either retained snapshot. */
enum { SFS64_BANK_SECTORS=8, SFS64_BANK_BYTES=4096, SFS64_JOURNAL_SECTORS=19 };
enum sj_status { SJ_OK,SJ_INVALID,SJ_SIGNATURE,SJ_CORRUPT,SJ_IO,SJ_UNCERTAIN,SJ_STALE,SJ_FULL };
struct sfs64_journal {
    struct bs_service *block;
    uint64_t owner,token,capacity,data_start;
    uint64_t generation[2];
    size_t length[2];
    unsigned active;
    bool valid;
    const void *writer; /* trusted stream transaction, never a user pointer */
    uint8_t payload[2][SFS64_BANK_BYTES];
};
bool sfs64_journal_ready(const struct sfs64_journal *,bool write);
bool sfs64_snapshot_valid(const uint8_t *,size_t,uint64_t capacity,uint64_t data_start);
/* Read only. Never initialize on error. On any transport error refuse
 * the mount rather than mistaking an unavailable bank for corruption. */
enum sj_status sfs64_journal_mount(struct sfs64_journal *,struct bs_service *,
                                   uint64_t owner,uint64_t token,uint64_t capacity,uint64_t data_start);
/* Exact retained generation, read-only grant required. */
enum sj_status sfs64_journal_mount_generation(struct sfs64_journal *,struct bs_service *,
 uint64_t owner,uint64_t token,uint64_t capacity,uint64_t data_start,uint64_t generation);
/* New payload must be a disjoint immutable trusted span. Write inactive
 * payload and confirm each sector, then durable header. Failure invalidates
 * the live journal. Reopen after recovery before deciding what committed. */
enum sj_status sfs64_journal_publish(struct sfs64_journal *,const uint8_t *,size_t);
#ifdef STYLEFS_HOST_FORMAT
/* Explicit provisioning of a freshly reserved journal region only.
 * Requires all 19 sectors zero, does not scan/format the data region.
 * Marker written last. Never called from mount or guest. */
enum sj_status sfs64_journal_provision(struct bs_service *,uint64_t owner,uint64_t token,
                                       uint64_t capacity,uint64_t data_start);
#endif
#endif
