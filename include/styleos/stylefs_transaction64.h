#ifndef STYLEOS_TRANSACTION64_H
#define STYLEOS_TRANSACTION64_H
#include <styleos/stylefs_checkpoint64.h>
/* Single trusted serialized supervisor; all raw journal operations are
 * private to it. Dedicated state and workspace, no concurrent mutation
 * or callback reentry. No automatic provisioning or reconciliation.
 * A failed/uncertain transaction locks writes; explicit recovery is RO.
 * Mount only an exact already-checkpointed pair. Never adopt a newer pair
 * without an existing receipt. This does not enforce an untrusted ABI. */
enum tx64_status {TX64_OK,TX64_INVALID,TX64_LOCKED,TX64_CONFLICT,TX64_FULL,
 TX64_CANCELLED,TX64_RECOVERY,TX64_IO,TX64_CORRUPT};
struct tx64_state {
 struct bs_service *block;struct cp64_layout layout;
 struct dc64_state dir;struct sfs64_journal data;
 struct ns64_proof proof;struct fb64_view view;struct cp64_result receipt;
 struct ns64_node *nodes;size_t slots;
 bool ready,busy;
};
/* state and inventory remain at stable addresses throughout the session. */
enum tx64_status tx64_open(struct tx64_state *,struct bs_service *,const struct cp64_layout *,
 struct ns64_node *,size_t,struct fb64_workspace *);
enum tx64_status tx64_create(struct tx64_state *,struct fb64_workspace *,
 const struct sd64_record *,uint64_t,fb64_source,void *);
#endif
