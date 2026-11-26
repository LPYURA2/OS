#ifndef STYLEOS_DIRECTORY64_H
#define STYLEOS_DIRECTORY64_H
#include <styleos/block_service.h>
/* Candidate immutable namespace index, not a mounted filesystem or ACL.
 * Parent object ID + exact UTF-8 name -> file/directory object ID.
 * Caller owns an immutable snapshot, validated metadata-region read grant,
 * trusted nonoverlapping buffers; no raw user pointers or concurrent writer.
 * Internal keys are inclusive child maxima, child pointers are grant-relative
 * sector addresses. CRC detects corruption, not malicious authentication. */
enum { SD64_PAGE=4096, SD64_SECTORS=8, SD64_FANOUT=14, SD64_NAME=255, SD64_MAX_LEVEL=8 };
enum sd64_kind { SD64_BRANCH=0, SD64_FILE=1, SD64_DIRECTORY=2 };
enum sd64_status { SD64_OK, SD64_MISSING, SD64_INVALID, SD64_CORRUPT, SD64_IO };
struct sd64_key {uint64_t parent;uint16_t length;uint8_t name[SD64_NAME];};
struct sd64_record {struct sd64_key key;uint64_t target;uint8_t kind;};
struct sd64_root {uint64_t sector,epoch,region_start,region_sectors;unsigned level;};
bool sd64_key_valid(const struct sd64_key *);
int sd64_compare(const struct sd64_key *,const struct sd64_key *);
/* Encode a canonical nonempty page. Caller provisions only fresh pages.
 * No writes, allocation, publication or mutation of retained snapshots. */
bool sd64_encode(uint8_t page[SD64_PAGE],uint64_t epoch,unsigned level,
                 const struct sd64_record *,unsigned count);
/* Read at most (SD64_MAX_LEVEL+1)*8 sectors, one page scratch, no heap.
 * Validates all entries on visited pages and child level/fence continuity.
 * Only returns an object ID, not authority to open it. No output on error. */
enum sd64_status sd64_lookup(struct bs_service *,uint64_t owner,uint64_t token,
    const struct sd64_root *,const struct sd64_key *,uint8_t scratch[SD64_PAGE],
    uint64_t *object,uint8_t *kind);
#endif
