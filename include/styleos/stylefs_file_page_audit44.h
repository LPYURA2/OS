#ifndef STYLEOS_FILE_PAGE_AUDIT44_H
#define STYLEOS_FILE_PAGE_AUDIT44_H
#include <styleos/stylefs_file_syscall43.h>
#include <styleos/user_root_846.h>
/* Trusted architecture-owned evidence, pinned for the entire syscall.
 * observed_safe_root must come from privileged CR3 capture; no CPU read
 * is performed here. Existing root audit checks actual table bytes,
 * lease identity, supervisor identity mapping and user leaf permissions.
 * Host fixtures are not hardware evidence. */
struct fs44_audit {
 const struct s842_root *safe;
 const struct s842_cache *cache;
 const struct s844_arena *arena;
 const struct s844_lease *leases;
 const struct s846_guard *guard;
 const struct s846_root *roots;
 uint64_t observed_safe_root;
};
bool fs44_audit_page(void *,const struct rt862_selection *,const uint8_t *);
#endif
