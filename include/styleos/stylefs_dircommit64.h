#ifndef STYLEOS_DIRCOMMIT64_H
#define STYLEOS_DIRCOMMIT64_H
#include <styleos/stylefs_directory64.h>
/* Dedicated two-bank metadata slice. Sector 0 and 1 are root receipts.
 * Remaining sectors comprise two equal page-aligned banks. No data extents
 * or ordinary filesystem formats are touched. Trusted serialized caller
 * owns sole write grant; explicitly provisioned blank region only.
 * A new complete sorted snapshot is built into the inactive bank. */
enum dc64_status { DC64_OK, DC64_INVALID, DC64_CORRUPT, DC64_IO, DC64_FULL, DC64_UNCERTAIN, DC64_CANCELLED };
struct dc64_state {
 struct bs_service *block;uint64_t owner,token,sectors,epoch,count;
 unsigned active;bool valid;
 struct sd64_root root;
};
struct dc64_workspace {
 uint8_t page[SD64_PAGE];
 struct sd64_record records[SD64_FANOUT],child[SD64_FANOUT];
};
typedef bool (*dc64_next)(void *,uint64_t,struct sd64_record *);
/* Mount validates both committed trees, including ordering and exact
 * deterministic child layout. Read failure is not evidence of corruption. */
enum dc64_status dc64_mount(struct dc64_state *,struct bs_service *,uint64_t owner,
                             uint64_t token,uint64_t sectors,struct dc64_workspace *);
/* Blank header initialization only; never erase or autoformat on mount. */
#ifdef STYLEFS_HOST_FORMAT
enum dc64_status dc64_provision(struct bs_service *,uint64_t owner,uint64_t token,uint64_t sectors);
#endif
/* next yields sorted file/directory records by index, false cancels.
 * Caller serializes and keeps the source immutable for entire operation.
 * On I/O uncertainty remount is required. No live page is overwritten. */
enum dc64_status dc64_publish(struct dc64_state *,struct dc64_workspace *,
                               uint64_t count,dc64_next,void *);
#endif
