#ifndef STYLEOS_CHECKPOINT64_H
#define STYLEOS_CHECKPOINT64_H
#include <styleos/stylefs_filebind64.h>
/* Two dedicated receipt sectors on same device, disjoint from directory/data.
 * Trusted serialized supervisor. Provision only a new blank slice. Persist
 * after a successful binding. This is explicit checkpoint/recovery, NOT an
 * automatic coordinated write API: at most one underlying commit to each
 * journal may occur before next checkpoint; recovery is strictly read-only.
 * No automatic repair, reformat, rollback writes or orphan reclamation. */
struct cp64_layout {uint64_t owner,receipt,dir,data,dir_sectors,data_sectors,data_start;};
struct cp64_result {uint64_t sequence,epoch,generation;bool fallback;};
enum cp64_status {CP64_OK,CP64_INVALID,CP64_IO,CP64_CORRUPT,CP64_STALE,CP64_FULL,CP64_UNCERTAIN};
/* Writes a receipt for a current validated pair. Read/write grants required.
 * Prior receipts must refer to this exact device and geometry. */
enum cp64_status cp64_save(struct bs_service *,const struct cp64_layout *,struct fb64_view *);
/* Recovery requires THREE read-only grants. Refuses transport errors; tries
 * the previous receipt only when the newer pair is structurally invalid.
 * All state, workspace and inventory buffers belong to the trusted caller.
 * Result and view valid only on success. */
enum cp64_status cp64_recover(struct bs_service *,const struct cp64_layout *,
 struct dc64_state *,struct sfs64_journal *,struct ns64_proof *,struct ns64_node *,size_t slots,
 struct dc64_workspace *,struct fb64_view *,struct cp64_result *);
#endif
