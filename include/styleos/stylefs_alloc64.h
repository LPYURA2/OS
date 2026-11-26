#ifndef STYLEOS_ALLOC64_H
#define STYLEOS_ALLOC64_H
#include <styleos/stylefs_map64.h>
/* Trusted allocator planning, not a disk reservation or a transaction.
 * Each list is the complete physically sorted inventory of one validated
 * snapshot, including other reserved data runs. Runs within a snapshot
 * must not overlap. Sharing between current and previous is allowed;
 * their union remains unavailable. Caller serializes planning and retains
 * exclusive ownership through durable commit/cancel. Arrays immutable and
 * disjoint from output; no user pointers or untrusted counts. */
struct sfs64_inventory {const struct sfs64_run *runs;size_t count;};
enum sfs64_alloc_status {SFS64_ALLOC_OK,SFS64_ALLOC_INVALID,SFS64_ALLOC_FULL,SFS64_ALLOC_OUTPUT};
struct sfs64_space {uint64_t free_sectors,largest_run;size_t gaps;};
enum sfs64_alloc_status sfs64_space_query(uint64_t capacity,uint64_t data_start,
    struct sfs64_inventory current,struct sfs64_inventory previous,struct sfs64_space *);
 /* First fit over free gaps, in physical order. Full plan or no publication.
  * Required run count returned on OK/OUTPUT only, and output unchanged on
  * failure. NULL with zero slots supports sizing. Zero byte file needs no run.
  * No artificial file-size cap beyond 64-bit bytes and available capacity. */
enum sfs64_alloc_status sfs64_allocate(uint64_t capacity,uint64_t data_start,
    struct sfs64_inventory current,struct sfs64_inventory previous,uint64_t bytes,
    struct sfs64_run *out,size_t slots,size_t *count);
#endif
