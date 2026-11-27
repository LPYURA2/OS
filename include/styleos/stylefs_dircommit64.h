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

/* A single trusted namespace operation, not a path/ACL/inode service.
 * CREATE requires unused key; REMOVE/RENAME require exact old object and kind.
 * RENAME keeps object identity and refuses replacement of an existing key.
 * Directory moves/removals are deliberately refused until parent integrity
 * and cycle/emptiness checks exist. Creation of entries does not allocate IDs.
 */
enum dc64_edit_op { DC64_CREATE=1, DC64_REMOVE, DC64_RENAME };
struct dc64_edit {
 enum dc64_edit_op op;
 struct sd64_record entry;
 struct sd64_key destination;
};
struct dc64_edit_workspace {
 struct dc64_workspace publish;
 uint8_t page[SD64_PAGE];
 struct sd64_record records[SD64_FANOUT];
};
/* Enumeration is indexed by ordinal in the mounted snapshot; caller serializes
 * and does not retain a snapshot across publication. No result on error.
 * Return DC64_INVALID for ordinal outside count. */
enum dc64_status dc64_entry_at(struct dc64_state *,uint64_t ordinal,
                                struct dc64_edit_workspace *,struct sd64_record *);
enum dc64_status dc64_apply(struct dc64_state *,struct dc64_edit_workspace *,
                             const struct dc64_edit *);

/* Checked hierarchy profile. Root ID 1 is implicit and reserved; each
 * nonroot object has one entry (no hard links in this profile).
 * Caller-owned bounded node scratch: one node per entry, plus one on create.
 * Maximum 64 path components. Raw snapshot APIs remain diagnostic only;
 * trusted supervisor serializes all operations and uses checked profile.
 * No inode allocation, ACL or data deletion is performed by these APIs. */
enum { DC64_PATH_MAX=64 };
struct dc64_node {uint64_t id,parent;uint8_t kind;};
struct dc64_path_part {uint16_t length;uint8_t name[SD64_NAME];};
enum dc64_status dc64_hierarchy_check(struct dc64_state *,struct dc64_edit_workspace *,
                                      struct dc64_node *,size_t capacity);
enum dc64_status dc64_hierarchy_apply(struct dc64_state *,struct dc64_edit_workspace *,
                                      struct dc64_node *,size_t capacity,const struct dc64_edit *);
enum dc64_status dc64_resolve(struct dc64_state *,struct dc64_edit_workspace *,
                              struct dc64_node *,size_t capacity,
                              const struct dc64_path_part *,unsigned count,
                              uint64_t *object,uint8_t *kind);
#endif
