#ifndef STYLEOS_DESKTOP_PAGED_VIEW_H
#define STYLEOS_DESKTOP_PAGED_VIEW_H
#include <styleos/stylefs_paged_object64.h>
#include <styleos/desktop_v0.h>
/* Trusted desktop-only read-only cached view; no disk grants or user ABI.
 * Bounded first page, explicit truncation, no filesystem I/O in renderer. */
enum { DP41_ROWS=16 };
struct dp41_view {struct po64_row rows[DP41_ROWS];unsigned count;bool more,valid;};
const struct dp41_view *s41_root_view(void);
void desk_fs_paged_root_draw(const struct dp41_view *,struct desk_box);
#endif
