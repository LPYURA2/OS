
#ifndef STYLEOS_INODE_PAGES64_H
#define STYLEOS_INODE_PAGES64_H
#include <styleos/stylefs_map64.h>
/* Immutable packed manifest pages, distinct from the live 4KiB journal.
 * Trusted serialized owner supplies exclusively reserved unpublished
 * metadata region, separate from referenced data. No publication, format,
 * allocation, user pointers or authority to open files in this module. */
enum { IP64_PAGE=4096, IP64_SECTORS=8, IP64_HEADER=64, IP64_MAP_MAX=4028 };
enum ip64_status { IP64_OK,IP64_MISSING,IP64_INVALID,IP64_CORRUPT,IP64_IO,IP64_FULL,IP64_CANCELLED,IP64_UNCERTAIN };
struct ip64_root {
 uint64_t start,sectors,epoch,pages,files,data_capacity,data_start;
};
struct ip64_workspace {uint8_t page[IP64_PAGE],map[IP64_MAP_MAX];};
typedef bool (*ip64_source)(void *,uint64_t ordinal,uint8_t *,size_t capacity,size_t *length);
/* Stream sorted maps into fresh region, never overwrite a retained page.
 * Returns root only after all writes, flushes and readback. Failure leaves
 * unpublished bytes; caller must not publish partial root. */
enum ip64_status ip64_build(struct bs_service *,uint64_t owner,uint64_t token,
 const struct ip64_root *geometry,uint64_t count,ip64_source,void *,
 struct ip64_workspace *,struct ip64_root *out);
/* Complete ordering and cross-file physical extent overlap validation.
 * One caller-supplied run per extent, not file contents; refuse capacity
 * exhaustion. This validates one snapshot, not retained-bank allocation. */
enum ip64_status ip64_validate(struct bs_service *,uint64_t owner,uint64_t token,
 const struct ip64_root *,struct ip64_workspace *,struct sfs64_run *,size_t capacity);
/* Same complete validation, plus exact count of sorted occupied runs.
 * Count output is unchanged on failure and must not alias any input. */
enum ip64_status ip64_inventory(struct bs_service *,uint64_t owner,uint64_t token,
 const struct ip64_root *,struct ip64_workspace *,struct sfs64_run *,size_t capacity,size_t *count);
/* Binary search after successful full validation of immutable root.
 * Return copied metadata, no borrowed pointers and no output on error. */
enum ip64_status ip64_stat(struct bs_service *,uint64_t owner,uint64_t token,
 const struct ip64_root *,uint64_t id,struct ip64_workspace *,struct sfs64_file *);
/* Copy exact encoded map into workspace.map; length unchanged on failure.
 * Requires previously validated immutable root. */
enum ip64_status ip64_export(struct bs_service *,uint64_t owner,uint64_t token,
 const struct ip64_root *,uint64_t id,struct ip64_workspace *,size_t *length);
#endif
